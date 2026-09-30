#include <gtest/gtest.h>

#include "cache/rd/recency_index.hpp"
#include "yarda/cache/exact_csrd.hpp"

namespace
{
using namespace yarda;

TEST(CsrdStatisticsTest, RejectsSnapshotWhenMeasurementWasDisabled)
{
  ExactCsrdAnalyzer analyzer({32, 2, 1});
  EXPECT_THROW(analyzer.statistics(), std::logic_error);
}

TEST(CsrdStatisticsTest, EmptyAnalyzerHasNoAllocatedSetStorage)
{
  ExactCsrdAnalyzer analyzer({32, 2, 1}, true);
  const auto stats = analyzer.statistics();
  EXPECT_EQ(stats.touched_sets, 0U);
  EXPECT_EQ(stats.active_history_entries, 0U);
  EXPECT_EQ(stats.allocated_fenwick_elements, 0U);
  EXPECT_EQ(stats.compaction_count, 0U);
}

TEST(CsrdStatisticsTest, FixedDomainRetainsBoundedStorageAfterManyCompactions)
{
  const CacheGeometry geometry{32, 2, 1};
  ExactCsrdAnalyzer measured(geometry, true), plain(geometry);
  for (std::uint64_t i = 0; i < 10000; ++i)
  {
    const auto address = decode_cache_address((i % 4) * 32, geometry);
    const auto actual = measured.observe(address);
    const auto expected = plain.observe(address);
    ASSERT_EQ(actual.distance, expected.distance);
    ASSERT_EQ(actual.outcome, expected.outcome);
  }
  const auto stats = measured.statistics();
  EXPECT_EQ(stats.touched_sets, 2U);
  EXPECT_EQ(stats.active_history_entries, 4U);
  EXPECT_EQ(stats.fenwick_slots, 32U);
  EXPECT_EQ(stats.allocated_fenwick_elements, 34U);
  EXPECT_EQ(stats.histogram_keys, 1U);
  EXPECT_GT(stats.compaction_count, 100U);
  EXPECT_GT(stats.maximum_compaction_scratch_bytes, 0U);
  EXPECT_EQ(measured.summary().histogram, plain.summary().histogram);
}

TEST(CsrdStatisticsTest, GrowingDomainCountsEveryHistoricalLine)
{
  const CacheGeometry geometry{32, 2, 1};
  ExactCsrdAnalyzer analyzer(geometry, true);
  for (std::uint64_t i = 0; i < 1024; ++i)
    analyzer.observe(decode_cache_address(i * 32, geometry));
  const auto stats = analyzer.statistics();
  EXPECT_EQ(stats.active_history_entries, 1024U);
  EXPECT_LE(stats.fenwick_slots, 2048U);
  EXPECT_LE(stats.allocated_fenwick_elements, 2050U);
  EXPECT_EQ(stats.histogram_keys, 0U);
  EXPECT_GT(stats.hash_buckets, 0U);
}

TEST(CsrdStatisticsTest, CountsOnlyActualCompactions)
{
  detail::RecencyIndex index(4, 100, true);
  for (const auto key : {0U, 1U, 0U, 1U}) index.observe(key);
  EXPECT_EQ(index.statistics().compaction_count, 0U);
  EXPECT_EQ(index.observe(0), 1U);
  const auto stats = index.statistics();
  EXPECT_EQ(stats.compaction_count, 1U);
  EXPECT_EQ(stats.active_history_entries, 2U);
  EXPECT_GE(stats.maximum_compaction_scratch_bytes,
            5 * sizeof(std::uint64_t) + 2 * sizeof(std::pair<std::uint64_t,
                                                          std::uint64_t>));
}
} // namespace
