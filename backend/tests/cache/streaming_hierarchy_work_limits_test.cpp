#include "../trace/work_limits_test_support.hpp"
#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;
using yarda::test::work::expect_limit_error;

TEST(StreamingHierarchyWorkLimitsTest, PassesSingleLoopAllowanceToProducer)
{
  const auto raw = byte_module(
    Json::array({function("kernel", Json::array({loop(3, byte_body({0}))}))}));
  StreamingHierarchyOptions options;
  options.loop_limits = {2, 100};
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                  options);
    },
    "loop iteration count exceeds 2");
}

TEST(StreamingHierarchyWorkLimitsTest,
     PassesCumulativeAllowanceToEmptyNestedLoops)
{
  const auto raw = byte_module(Json::array({function(
    "kernel",
    Json::array({loop(2, Json::array({loop(3, Json::array(), "j")}))}))}));
  StreamingHierarchyOptions options;
  options.loop_limits = {3, 7};
  options.emission_limits = {100'000'000, 100'000'000};
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                  options);
    },
    "cumulative loop iteration count exceeds 7");
  options.loop_limits.cumulative_loop_iterations = 8;
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 1U);
  stream::expect_coverage(result.coverage, {});
  EXPECT_TRUE(result.tasks[0].invariants.all_passed);
}

TEST(StreamingHierarchyWorkLimitsTest,
     PreservesColdTasksWithSharedLoopAllowance)
{
  auto region = function("region", Json::array({loop(2, byte_body({0}))}));
  region["analysis_scope"] = {{"kind", "region"}, {"name", "APE_ANALYZE"}};
  const auto raw = byte_module(Json::array({
    function("first", Json::array({loop(2, byte_body({0}))})),
    region,
  }));
  EventCollector collector;
  auto options = collector.options(4, {4, 4});
  options.loop_limits = {2, 4};
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 2U);
  EXPECT_EQ(result.tasks[0].task_id, "first");
  EXPECT_EQ(result.tasks[1].task_id, "region:6:region:APE_ANALYZE");
  for (const auto & task : result.tasks)
  {
    EXPECT_EQ(task.source_accesses, 2U);
    EXPECT_EQ(task.ehc_l1, 1U);
    EXPECT_EQ(task.ehc_llc, 0U);
    EXPECT_EQ(task.all_cache_misses, 1U);
    EXPECT_TRUE(task.invariants.all_passed);
  }
  stream::expect_coverage(result.coverage, {4, 4, 0, 4});
  ASSERT_EQ(collector.events.size(), 4U);
  EXPECT_EQ(collector.events[2].second.l1_mapping.source_access_ordinal, 0U);
}

TEST(StreamingHierarchyWorkLimitsTest,
     DiscardsModuleResultOnLaterLoopExhaustion)
{
  const auto body = Json::array({loop(2, byte_body({0}))});
  const auto raw = byte_module(
    Json::array({function("first", body), function("second", body)}));
  EventCollector collector;
  auto options = collector.options(10, {4, 4});
  options.loop_limits = {2, 3};
  bool returned = false;
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                  options);
      returned = true;
    },
    "cumulative loop iteration count exceeds 3");
  EXPECT_FALSE(returned);
  ASSERT_EQ(collector.events.size(), 2U);
  EXPECT_EQ(collector.events.back().first, "first");
}

TEST(StreamingHierarchyWorkLimitsTest,
     ChargesCrossLineAccessesWithoutLlcDoubleCharge)
{
  const auto raw = stream::module(
    Json::array(
      {function("kernel", Json::array({loop(2, Json::array({access()}))}))}),
    8);
  StreamingHierarchyOptions options;
  options.loop_limits = {2, 2};
  options.emission_limits = {2, 4};
  const auto result = analyze_streaming_hierarchy(
    raw, stream::addresses(8), make_batch_hierarchy(), options);
  stream::expect_coverage(result.coverage, {2, 2, 0, 4});
  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].l1.lookups, 4U);
  EXPECT_EQ(result.tasks[0].llc.lookups, 2U);
  EXPECT_EQ(result.tasks[0].ehc_l1, 2U);
  EXPECT_EQ(result.tasks[0].all_cache_misses, 2U);
}

TEST(StreamingHierarchyWorkLimitsTest,
     LineFailureStillStopsInsideSpanWithCustomLoops)
{
  const auto raw = stream::module(
    Json::array(
      {function("kernel", Json::array({loop(2, Json::array({access()}))}))}),
    8);
  EventCollector collector;
  auto options = collector.options(10, {2, 3});
  options.loop_limits = {2, 2};
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, stream::addresses(8),
                                  make_batch_hierarchy(), options);
    },
    "emitted line references exceeds 3");
  ASSERT_EQ(collector.events.size(), 3U);
  EXPECT_EQ(collector.events.back().second.l1_mapping.source_access_ordinal,
            1U);
  EXPECT_EQ(collector.events.back().second.l1_mapping.line_span_ordinal, 0U);
}

TEST(StreamingHierarchyWorkLimitsTest,
     SourceFailureStillAppliesWithSufficientLoops)
{
  const auto raw = byte_module(
    Json::array({function("kernel", Json::array({loop(2, byte_body({0}))}))}));
  StreamingHierarchyOptions options;
  options.loop_limits = {2, 2};
  options.emission_limits = {1, 100};
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                  options);
    },
    "emitted source accesses exceeds 1");
}

TEST(StreamingHierarchyWorkLimitsTest,
     EventTruncationDoesNotSuppressLoopFailure)
{
  const auto raw = byte_module(Json::array(
    {function("kernel", Json::array({access(), loop(1, Json::array())}))}));
  EventCollector collector;
  auto options = collector.options(0);
  options.loop_limits = {0, 0};
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                  options);
    },
    "loop iteration count exceeds 0");
  EXPECT_TRUE(collector.events.empty());
}

TEST(StreamingHierarchyWorkLimitsTest,
     NextInvocationStartsFreshAfterLoopFailure)
{
  const auto raw = byte_module(
    Json::array({function("kernel", Json::array({loop(1, byte_body({0})),
                                                 loop(1, byte_body({0}))}))}));
  StreamingHierarchyOptions options;
  options.loop_limits = {1, 1};
  EXPECT_THROW(analyze_streaming_hierarchy(raw, byte_addresses(),
                                           make_batch_hierarchy(), options),
               std::invalid_argument);
  options.loop_limits.cumulative_loop_iterations = 2;
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].l1.cold_misses, 1U);
  EXPECT_EQ(result.tasks[0].ehc_l1, 1U);
}

TEST(StreamingHierarchyWorkLimitsTest,
     RaisedLoopAllowancesReachHierarchyExecution)
{
  const auto raw = byte_module(Json::array({function(
    "kernel", Json::array({loop(1'000'001, Json::array()), access()}))}));
  StreamingHierarchyOptions options;
  options.loop_limits = {1'000'001, 1'000'001};
  options.emission_limits = {1, 1};
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 1U);
  stream::expect_coverage(result.coverage, {1, 1, 0, 1});
  EXPECT_EQ(result.tasks[0].all_cache_misses, 1U);
}

}  // namespace
