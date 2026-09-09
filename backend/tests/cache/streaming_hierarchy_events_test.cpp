#include <stdexcept>

#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;

class StreamingHierarchyEventLimitTest
  : public ::testing::TestWithParam<std::uint64_t>
{
};

TEST_P(StreamingHierarchyEventLimitTest,
       BoundsModulePrefixWithoutChangingSummary)
{
  const auto raw = byte_module(Json::array({
    function("first", byte_body({0, 32, 64})),
    function("empty", Json::array()),
    function("second", byte_body({0, 0})),
  }));
  const auto objects = byte_addresses();
  const auto hierarchy = make_batch_hierarchy({32, 2, 2}, {32, 4, 2});
  const auto baseline =
    analyze_batch_hierarchy(resolved_task_traces(raw, objects), hierarchy);
  EventCollector collector;
  const auto result = analyze_streaming_hierarchy(
    raw, objects, hierarchy, collector.options(GetParam()));
  const auto retained = std::min<std::uint64_t>(GetParam(), 5);
  ASSERT_EQ(collector.events.size(), retained);
  EXPECT_EQ(result.event_delivery.emitted_events, retained);
  EXPECT_EQ(result.event_delivery.events_truncated, GetParam() < 5);
  stream::expect_coverage(result.coverage, (TraceCoverage{5, 5, 0, 5}));
  ASSERT_EQ(result.tasks.size(), baseline.tasks.size());
  std::size_t index = 0;
  for (std::size_t task = 0; task < baseline.tasks.size(); ++task)
  {
    expect_summary(result.tasks[task], baseline.tasks[task].summary);
    for (const auto & expected : baseline.tasks[task].events)
    {
      if (index == collector.events.size()) break;
      EXPECT_EQ(collector.events[index].first,
                baseline.tasks[task].summary.task_id);
      expect_event(collector.events[index++].second, expected);
    }
  }
  EXPECT_EQ(index, retained);
}

INSTANTIATE_TEST_SUITE_P(Boundaries, StreamingHierarchyEventLimitTest,
                         ::testing::Values(0U, 1U, 3U, 4U, 5U, 6U));

TEST(StreamingHierarchyEventsTest, EmptyTasksDoNotMarkZeroLimitTruncated)
{
  EventCollector collector;
  const auto result =
    analyze_streaming_hierarchy(byte_trace({}), byte_addresses(),
                                make_batch_hierarchy(), collector.options(0));
  EXPECT_TRUE(collector.events.empty());
  EXPECT_EQ(result.event_delivery.emitted_events, 0U);
  EXPECT_FALSE(result.event_delivery.events_truncated);
}

TEST(StreamingHierarchyEventsTest, L1HitHasNoLlcPayloadAndColdHasNoDistance)
{
  EventCollector collector;
  static_cast<void>(
    analyze_streaming_hierarchy(byte_trace({0, 0}), byte_addresses(),
                                make_batch_hierarchy(), collector.options()));
  ASSERT_EQ(collector.events.size(), 2U);
  const auto & cold = collector.events[0].second;
  EXPECT_FALSE(cold.l1.reuse_distance);
  ASSERT_TRUE(cold.llc);
  EXPECT_FALSE(cold.llc->reuse_distance);
  EXPECT_EQ(cold.first_service, FirstServiceLevel::Memory);
  const auto & hit = collector.events[1].second;
  EXPECT_EQ(hit.l1.reuse_distance, 0U);
  EXPECT_FALSE(hit.llc);
  EXPECT_FALSE(hit.llc_mapping);
  EXPECT_EQ(hit.first_service, FirstServiceLevel::L1);
}

TEST(StreamingHierarchyEventsTest, RemapsOriginalAddressWithLlcGeometry)
{
  EventCollector collector;
  static_cast<void>(analyze_streaming_hierarchy(
    byte_trace({65}), byte_addresses(),
    make_batch_hierarchy({32, 2, 2}, {32, 8, 2}), collector.options()));
  ASSERT_EQ(collector.events.size(), 1U);
  const auto & event = collector.events[0].second;
  EXPECT_EQ(event.l1_mapping.decoded.set_index, 0U);
  EXPECT_EQ(event.l1_mapping.decoded.tag, 2U);
  ASSERT_TRUE(event.llc_mapping);
  EXPECT_EQ(event.llc_mapping->decoded.address, 65U);
  EXPECT_EQ(event.llc_mapping->decoded.set_index, 2U);
  EXPECT_EQ(event.llc_mapping->decoded.tag, 0U);
  EXPECT_EQ(event.llc_mapping->decoded.line_offset, 1U);
  test::support::expect_batch_provenance(event.l1_mapping, *event.llc_mapping);
}

TEST(StreamingHierarchyEventsTest, DeliversBeforeResolvingTheNextSource)
{
  const auto raw = byte_module(Json::array({function("kernel", Json::array({
                                                                 access(),
                                                                 access("global"
                                                                        "::"
                                                                        "missin"
                                                                        "g"),
                                                               }))}));
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(raw, byte_addresses(),
                                           make_batch_hierarchy(),
                                           collector.options()),
               ResolutionError);
  ASSERT_EQ(collector.events.size(), 1U);
  EXPECT_EQ(collector.events[0].second.first_service,
            FirstServiceLevel::Memory);
}

TEST(StreamingHierarchyEventsTest,
     StopsCallingSinkAtLimitWhileCompletingAnalysis)
{
  auto options = StreamingHierarchyOptions{};
  options.event_limit = 1;
  unsigned count = 0;
  options.event_sink = [&](const std::string &, const HierarchyAccessEvent &) {
    if (++count > 1) throw std::runtime_error("unexpected diagnostic delivery");
  };
  const auto result =
    analyze_streaming_hierarchy(byte_trace({0, 0, 32, 0}), byte_addresses(),
                                make_batch_hierarchy(), options);
  EXPECT_EQ(count, 1U);
  EXPECT_TRUE(result.event_delivery.events_truncated);
  EXPECT_EQ(result.coverage.emitted_line_references, 4U);
  EXPECT_EQ(result.tasks.at(0).ehc_l1, 2U);
  EXPECT_EQ(result.tasks.at(0).all_cache_misses, 2U);
}

TEST(StreamingHierarchyEventsTest, RejectsNonzeroLimitWithoutSink)
{
  StreamingHierarchyOptions options;
  options.event_limit = 1;
  EXPECT_THROW(analyze_streaming_hierarchy(byte_trace({0}), byte_addresses(),
                                           make_batch_hierarchy(), options),
               std::invalid_argument);
}

}  // namespace
