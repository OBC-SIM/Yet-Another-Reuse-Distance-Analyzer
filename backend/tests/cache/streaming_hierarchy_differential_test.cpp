#include "hierarchy_lru_oracle_differential_support.hpp"
#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;

TEST(StreamingHierarchyDifferentialTest,
     MatchesBatchAndIndependentSeededOracles)
{
  for (const auto & spec : test::support::seeded_oracle_trace_specs())
  {
    const auto trace = test::support::make_seeded_oracle_trace(spec);
    SCOPED_TRACE(test::support::describe_seeded_oracle_trace(spec, trace));
    auto body = Json::array();
    for (const auto & row : trace.accesses)
      body.push_back(
        access("global::A", std::to_string(row.decoded.address),
               row.operation == AccessOperation::Load ? "load" : "store"));
    const auto raw = byte_module(Json::array({
      function(trace.task_id, body),
      function("empty", Json::array()),
      function("repeated", body),
    }));
    expect_stream_parity(
      raw, byte_addresses(),
      make_batch_hierarchy(spec.l1_geometry, spec.llc_geometry));
  }
}

TEST(StreamingHierarchyDifferentialTest, MatchesMixedCrossLineGeometryCases)
{
  for (const auto line : {32U, 64U})
  {
    SCOPED_TRACE(line);
    const auto raw = stream::module(
      Json::array({function("wide", Json::array({
                                      access(),
                                      access("global::B", "1", "store"),
                                      access("global::A", "2"),
                                      access("global::A", "0", "store"),
                                      access("global::B", "1"),
                                    }))}),
      line + 8);
    expect_stream_parity(raw, stream::addresses(line + 8),
                         make_batch_hierarchy({line, 4, 2}, {line, 8, 2}));
  }
}

TEST(StreamingHierarchyDifferentialTest, PreservesInlineAndNestedLoopOrder)
{
  const auto raw = byte_module(Json::array({
    function("helper", byte_body({32, 64}), "ape.inline"),
    function("zeta",
             Json::array({loop(3, Json::array({
                                    access(),
                                    loop(2, Json::array({call("helper")}), "j"),
                                  }))})),
    function("empty", Json::array()),
    function("alpha", byte_body({32, 32})),
  }));
  expect_stream_parity(raw, byte_addresses(),
                       make_batch_hierarchy({32, 2, 2}, {32, 4, 2}));
}

TEST(StreamingHierarchyDifferentialTest, PreservesLoadAndStoreResidency)
{
  const auto raw = byte_module(Json::array({
    function("loads", byte_body({0, 32, 0, 64, 0, 32})),
    function("stores", byte_body({0, 32, 0, 64, 0, 32}, "store")),
  }));
  const auto hierarchy = make_batch_hierarchy({32, 2, 2}, {32, 2, 2});
  expect_stream_parity(raw, byte_addresses(), hierarchy);
  const auto result =
    analyze_streaming_hierarchy(raw, byte_addresses(), hierarchy);
  ASSERT_EQ(result.tasks.size(), 2U);
  auto expected = result.tasks[0];
  expected.task_id = "stores";
  expect_summary(result.tasks[1], expected);
}

TEST(StreamingHierarchyDifferentialTest, PreservesIndependentNonInclusiveLevels)
{
  const auto hierarchy = make_batch_hierarchy({32, 4, 2}, {32, 2, 1});
  // An LLC eviction of block 0 must not invalidate the surviving L1 copy.
  expect_stream_parity(byte_trace({0, 64, 0}), byte_addresses(), hierarchy);
  // L1 hits/evictions never refresh LLC recency or insert victims.
  expect_stream_parity(byte_trace({0, 32, 0, 64, 96, 128, 0, 32}),
                       byte_addresses(),
                       make_batch_hierarchy({32, 2, 2}, {32, 4, 2}));
}

TEST(StreamingHierarchyDifferentialTest,
     MatchesExplicitShadowedLoopSourcesAndBothIndependentOracles)
{
  const auto read = access("global::A", "i");
  const auto write = access("global::A", "i", "store");
  const auto body =
    Json::array({loop(3, Json::array({
                           read,
                           loop(2, Json::array({write}), "i", 1),
                           read,
                         }))});
  const auto raw = stream::module(Json::array({
                                    function("first", body),
                                    function("second", body),
                                  }),
                                  8);
  const auto objects = stream::addresses(8);
  const auto hierarchy = make_batch_hierarchy({32, 4, 2}, {32, 8, 2});

  // These offsets follow lexical loop scope, without consulting a producer.
  ResolvedTaskTrace first;
  first.task_id = "first";
  const std::vector<std::uint64_t> offsets{0, 8, 0, 8, 8, 8, 16, 8, 16};
  for (std::size_t i = 0; i < offsets.size(); ++i)
    test::support::append_batch_access(first, 0x101c + offsets[i], 8,
                                       i % 3 == 1 ? AccessOperation::Store
                                                  : AccessOperation::Load,
                                       "global::A", offsets[i]);
  auto second = first;
  second.task_id = "second";
  const auto expected_sources = test::support::batch_input({first, second});
  test::support::expect_batch_matches_oracles(expected_sources, hierarchy);
  const auto expected = analyze_batch_hierarchy(expected_sources, hierarchy);

  EventCollector collector;
  auto options = collector.options(22, {18, 22});
  options.loop_limits = {3, 12};
  const auto actual =
    analyze_streaming_hierarchy(raw, objects, hierarchy, options);
  stream::expect_coverage(actual.coverage, {18, 18, 0, 22});
  ASSERT_EQ(actual.tasks.size(), 2U);
  ASSERT_EQ(expected.tasks.size(), 2U);
  ASSERT_EQ(collector.events.size(), 22U);
  std::size_t event_index = 0;
  for (std::size_t task = 0; task < 2; ++task)
  {
    expect_summary(actual.tasks[task], expected.tasks[task].summary);
    for (const auto & event : expected.tasks[task].events)
    {
      ASSERT_LT(event_index, collector.events.size());
      EXPECT_EQ(collector.events[event_index].first,
                expected.tasks[task].summary.task_id);
      expect_event(collector.events[event_index++].second, event);
    }
  }
  EXPECT_EQ(event_index, 22U);
  EXPECT_EQ(actual.event_delivery.emitted_events, 22U);
  EXPECT_FALSE(actual.event_delivery.events_truncated);
}

}  // namespace
