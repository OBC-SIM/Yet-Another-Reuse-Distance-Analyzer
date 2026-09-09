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

}  // namespace
