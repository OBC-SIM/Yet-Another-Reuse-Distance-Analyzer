#include <stdexcept>

#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;

TEST(StreamingHierarchyBudgetTest, EmptyTasksFitZeroSourceAndLineLimits)
{
  StreamingHierarchyOptions options;
  options.emission_limits = {0, 0};
  const auto result = analyze_streaming_hierarchy(
    byte_trace({}), byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 1U);
  stream::expect_coverage(result.coverage, {});
  EXPECT_TRUE(result.tasks[0].invariants.all_passed);
}

TEST(StreamingHierarchyBudgetTest, AllowsExactSourceAndLineBoundaryAcrossTasks)
{
  const auto raw = byte_module(Json::array({
    function("first", byte_body({0, 32})),
    function("second", byte_body({0})),
  }));
  EventCollector collector;
  const auto result =
    analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                collector.options(3, {3, 3}));
  ASSERT_EQ(result.tasks.size(), 2U);
  stream::expect_coverage(result.tasks[0].coverage,
                          (TraceCoverage{2, 2, 0, 2}));
  stream::expect_coverage(result.tasks[1].coverage,
                          (TraceCoverage{1, 1, 0, 1}));
  stream::expect_coverage(result.coverage, (TraceCoverage{3, 3, 0, 3}));
  EXPECT_EQ(collector.events.size(), 3U);
}

TEST(StreamingHierarchyBudgetTest, RejectsCumulativeSourceLimitOnLaterTask)
{
  const auto raw = byte_module(Json::array({
    function("first", byte_body({0, 32})),
    function("second", byte_body({0, 0})),
  }));
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(raw, byte_addresses(),
                                           make_batch_hierarchy(),
                                           collector.options(10, {3, 10})),
               std::invalid_argument);
  ASSERT_EQ(collector.events.size(), 3U);
  EXPECT_EQ(collector.events[2].first, "second");
  EXPECT_EQ(collector.events[2].second.l1_mapping.source_access_ordinal, 0U);
}

TEST(StreamingHierarchyBudgetTest, RejectsLineLimitAcrossTasksInsideASpan)
{
  const auto raw = stream::module(Json::array({
                                    function("first", Json::array({access()})),
                                    function("second", Json::array({access()})),
                                  }),
                                  40);
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(raw, stream::addresses(40),
                                           make_batch_hierarchy(),
                                           collector.options(10, {2, 4})),
               std::invalid_argument);
  ASSERT_EQ(collector.events.size(), 4U);
  EXPECT_EQ(collector.events[2].first, "first");
  EXPECT_EQ(collector.events[2].second.l1_mapping.line_span_ordinal, 2U);
  EXPECT_EQ(collector.events[3].first, "second");
  EXPECT_EQ(collector.events[3].second.l1_mapping.line_span_ordinal, 0U);
}

TEST(StreamingHierarchyBudgetTest,
     DoesNotChargeLlcForwardingAsAnotherSourceLine)
{
  StreamingHierarchyOptions options;
  options.emission_limits = {2, 2};
  const auto result = analyze_streaming_hierarchy(
    byte_trace({0, 32}), byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].l1.lookups, 2U);
  EXPECT_EQ(result.tasks[0].llc.lookups, 2U);
  EXPECT_EQ(result.coverage.emitted_line_references, 2U);
}

TEST(StreamingHierarchyBudgetTest, RejectsSourceLimitInsideNestedLoops)
{
  const auto raw = byte_module(Json::array(
    {function("kernel", Json::array({
                          loop(2, Json::array({loop(3, byte_body({0}), "j")})),
                        }))}));
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(raw, byte_addresses(),
                                           make_batch_hierarchy(),
                                           collector.options(10, {5, 10})),
               std::invalid_argument);
  EXPECT_EQ(collector.events.size(), 5U);
}

TEST(StreamingHierarchyBudgetTest, RejectsHugeSpanAfterOnlyBudgetedLines)
{
  constexpr std::uint64_t size = std::uint64_t{1} << 40;
  const auto raw = stream::module(
    Json::array({function("wide", Json::array({access()}))}), size);
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(raw, stream::addresses(size),
                                           make_batch_hierarchy(),
                                           collector.options(10, {1, 2})),
               std::invalid_argument);
  EXPECT_EQ(collector.events.size(), 2U);
}

TEST(StreamingHierarchyBudgetTest, EnforcesLineLimitAfterEventTruncation)
{
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(
                 byte_trace({0, 0, 0}), byte_addresses(),
                 make_batch_hierarchy(), collector.options(1, {3, 2})),
               std::invalid_argument);
  EXPECT_EQ(collector.events.size(), 1U);
}

TEST(StreamingHierarchyBudgetTest, SummaryModeStillChargesLineBudget)
{
  StreamingHierarchyOptions options;
  options.emission_limits = {1, 0};
  EXPECT_THROW(analyze_streaming_hierarchy(byte_trace({0}), byte_addresses(),
                                           make_batch_hierarchy(), options),
               std::invalid_argument);
}

TEST(StreamingHierarchyBudgetTest, PreservesStructuralLoopGuardForEmptyBody)
{
  const auto raw = byte_module(Json::array({function(
    "kernel", Json::array({
                loop(1001, Json::array({loop(1000, Json::array(), "j")})),
              }))}));
  EXPECT_THROW(
    analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy()),
    std::invalid_argument);
}

TEST(StreamingHierarchyBudgetTest, FreshInvocationHasFreshBudgetAndColdState)
{
  StreamingHierarchyOptions options;
  options.emission_limits = {1, 1};
  EXPECT_THROW(analyze_streaming_hierarchy(byte_trace({0, 0}), byte_addresses(),
                                           make_batch_hierarchy(), options),
               std::invalid_argument);
  const auto result = analyze_streaming_hierarchy(
    byte_trace({0}), byte_addresses(), make_batch_hierarchy(), options);
  EXPECT_EQ(result.tasks.at(0).all_cache_misses, 1U);
  EXPECT_EQ(result.tasks.at(0).l1.cold_misses, 1U);
}

}  // namespace
