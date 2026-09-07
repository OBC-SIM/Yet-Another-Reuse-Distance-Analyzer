#include "hierarchy_level_analysis.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace
{

class BatchHierarchyLevelSummaryTest : public testing::Test
{
protected:
  void SetUp() override
  {
    mappings.resize(3);
    mappings[1].decoded.block_number = 1;
    analysis.accesses = {{yarda::LruAccessOutcome::ColdMiss, std::nullopt},
                         {yarda::LruAccessOutcome::ColdMiss, std::nullopt},
                         {yarda::LruAccessOutcome::Hit, 1U}};
    analysis.hits = 1;
    analysis.cold_misses = 2;
    analysis.histogram = {{1, 1}};
  }

  yarda::LruRdAnalysis analysis;
  std::vector<yarda::CacheLineMapping> mappings;
};

using yarda::detail::summarize_hierarchy_level;

TEST_F(BatchHierarchyLevelSummaryTest, PreservesCompleteCountsAndHistogram)
{
  const auto result = summarize_hierarchy_level(analysis, mappings);
  EXPECT_EQ(result.lookups, 3U);
  EXPECT_EQ(result.hits, 1U);
  EXPECT_EQ(result.misses, 2U);
  EXPECT_EQ(result.cold_misses, 2U);
  EXPECT_EQ(result.replacement_misses, 0U);
  EXPECT_EQ(result.unique_lines, 2U);
  EXPECT_EQ(result.csrd_histogram,
            (std::map<std::uint64_t, std::uint64_t>{{1, 1}}));
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsMisalignedAccessResults)
{
  analysis.accesses.pop_back();
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings), std::logic_error);
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsNonconservingLevelCounts)
{
  analysis.hits = 2;
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings), std::logic_error);
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsMissCountOverflow)
{
  analysis.cold_misses = std::numeric_limits<std::uint64_t>::max();
  analysis.replacement_misses = 1;
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings),
               std::overflow_error);
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsLookupCountOverflow)
{
  analysis.hits = std::numeric_limits<std::uint64_t>::max();
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings),
               std::overflow_error);
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsHistogramCountOverflow)
{
  analysis.histogram = {{0, std::numeric_limits<std::uint64_t>::max()}, {1, 1}};
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings),
               std::overflow_error);
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsIncompleteFiniteHistogram)
{
  analysis.histogram.clear();
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings), std::logic_error);
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsUniqueLineAndColdCountMismatch)
{
  mappings[1].decoded.block_number = 0;
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings), std::logic_error);
}

TEST_F(BatchHierarchyLevelSummaryTest, RejectsUniqueLinesExceedingColdMisses)
{
  mappings[2].decoded.block_number = 2;
  EXPECT_THROW(summarize_hierarchy_level(analysis, mappings), std::logic_error);
}

}  // namespace
