#include "streaming_hierarchy_summary.hpp"

#include <limits>
#include <stdexcept>

#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;
constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();

TEST(StreamingHierarchySummaryTest, PreservesFullHistogramAndOwnsItsStorage)
{
  ExactCsrdSummary source{7, 2, 3, 2, 3, {{0, 1}, {1, 1}, {17, 2}}};
  const auto result = detail::summarize_streaming_level(source);
  source.histogram.clear();
  expect_level(
    result, (CacheLevelSummary{7, 2, 5, 3, 2, 3, {{0, 1}, {1, 1}, {17, 2}}}));
}

TEST(StreamingHierarchySummaryTest, AcceptsEmptyLevel)
{
  expect_level(detail::summarize_streaming_level({}), {});
}

TEST(StreamingHierarchySummaryTest, RejectsNonConservingLevelCounts)
{
  EXPECT_THROW(detail::summarize_streaming_level({2, 0, 1, 0, 1, {}}),
               std::logic_error);
}

TEST(StreamingHierarchySummaryTest, RejectsIncompleteFiniteHistogram)
{
  EXPECT_THROW(detail::summarize_streaming_level({3, 1, 1, 1, 1, {{0, 1}}}),
               std::logic_error);
}

TEST(StreamingHierarchySummaryTest, RejectsUniqueLinesDifferentFromColdCount)
{
  EXPECT_THROW(detail::summarize_streaming_level({1, 0, 1, 0, 2, {}}),
               std::logic_error);
}

TEST(StreamingHierarchySummaryTest, RejectsMissCountOverflow)
{
  EXPECT_THROW(
    detail::summarize_streaming_level({maximum, 0, maximum, 1, maximum, {}}),
    std::overflow_error);
}

TEST(StreamingHierarchySummaryTest, RejectsLookupConservationOverflow)
{
  EXPECT_THROW(
    detail::summarize_streaming_level({maximum, maximum, 1, 0, 1, {}}),
    std::overflow_error);
}

TEST(StreamingHierarchySummaryTest, RejectsFiniteHistogramCountOverflow)
{
  EXPECT_THROW(detail::summarize_streaming_level(
                 {maximum, maximum, 0, 0, 0, {{0, maximum}, {1, 1}}}),
               std::overflow_error);
}

TEST(StreamingHierarchySummaryTest, AcceptsLargestRepresentableValidCounts)
{
  const auto result = detail::summarize_streaming_level(
    {maximum, maximum - 1, 1, 0, 1, {{0, maximum - 1}}});
  EXPECT_EQ(result.lookups, maximum);
  EXPECT_EQ(result.hits, maximum - 1);
  EXPECT_EQ(result.misses, 1U);
}

TEST(StreamingHierarchySummaryTest,
     AggregatesSourceAndLineCoverageIndependently)
{
  TraceCoverage total{2, 2, 0, 6};
  detail::add_streaming_coverage(total, {1, 1, 0, 2});
  stream::expect_coverage(total, (TraceCoverage{3, 3, 0, 8}));
}

class StreamingHierarchyCoverageOverflowTest
  : public ::testing::TestWithParam<int>
{
};

TEST_P(StreamingHierarchyCoverageOverflowTest,
       RejectsEachCumulativeCounterOverflow)
{
  TraceCoverage total;
  const auto field = GetParam();
  if (field == 0) total.source_accesses = maximum;
  if (field == 1) total.resolved_accesses = maximum;
  if (field == 2) total.rejected_accesses = maximum;
  if (field == 3) total.emitted_line_references = maximum;
  EXPECT_THROW(detail::add_streaming_coverage(total, {1, 1, 1, 1}),
               std::overflow_error);
}

INSTANTIATE_TEST_SUITE_P(Counters, StreamingHierarchyCoverageOverflowTest,
                         ::testing::Values(0, 1, 2, 3));

TEST(StreamingHierarchySummaryTest,
     ConvertsColdAndLargeFiniteObservationsExactly)
{
  const auto cold =
    detail::streaming_lru_result({LruAccessOutcome::ColdMiss, std::nullopt});
  EXPECT_EQ(cold.outcome, LruAccessOutcome::ColdMiss);
  EXPECT_FALSE(cold.reuse_distance);
  const auto hit = detail::streaming_lru_result({LruAccessOutcome::Hit, 0});
  EXPECT_EQ(hit.outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(hit.reuse_distance, 0U);
  const auto miss =
    detail::streaming_lru_result({LruAccessOutcome::ReplacementMiss, 17});
  EXPECT_EQ(miss.outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(miss.reuse_distance, 17U);
}

TEST(StreamingHierarchySummaryTest, HandlesDiagnosticDistanceAtPlatformLimit)
{
  const auto distance =
    static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
  const auto result =
    detail::streaming_lru_result({LruAccessOutcome::ReplacementMiss, distance});
  EXPECT_EQ(result.reuse_distance, distance);
  if constexpr (std::numeric_limits<std::size_t>::digits < 64)
  {
    EXPECT_THROW(detail::streaming_lru_result(
                   {LruAccessOutcome::ReplacementMiss, distance + 1}),
                 std::overflow_error);
  }
}

}  // namespace
