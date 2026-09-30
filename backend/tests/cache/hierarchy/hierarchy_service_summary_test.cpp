#include "cache/hierarchy/hierarchy_service_summary.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

#include "hierarchy_aggregation_test_support.hpp"

namespace
{

using yarda::detail::finalize_hierarchy_service;
using yarda::test::support::expect_hierarchy_invariants;

class HierarchyServiceSummaryTest : public testing::Test
{
protected:
  void SetUp() override
  {
    const auto fixture = yarda::test::support::make_aggregation_fixture();
    summary.task_id = fixture.task_id;
    summary.source_accesses = 5;
    summary.modeled_accesses = 5;
    summary.l1 = fixture.l1.summary;
    summary.llc = fixture.llc.summary;
    summary.l1_first_hit_count = 1;
    summary.llc_first_hit_count = 1;
    summary.all_cache_misses = 3;
    summary.coverage = fixture.coverage;
  }

  yarda::TaskHierarchySummary summary;
  static constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
};

TEST_F(HierarchyServiceSummaryTest, PreservesCountsCoverageAndHistograms)
{
  const auto result = finalize_hierarchy_service(summary);
  EXPECT_EQ(result.task_id, "service");
  EXPECT_EQ(result.source_accesses, 5U);
  EXPECT_EQ(result.modeled_accesses, 5U);
  EXPECT_EQ(result.l1_first_hit_count, 1U);
  EXPECT_EQ(result.llc_first_hit_count, 1U);
  EXPECT_EQ(result.all_cache_misses, 3U);
  EXPECT_EQ(result.coverage.source_accesses, 5U);
  EXPECT_EQ(result.coverage.resolved_accesses, 5U);
  EXPECT_EQ(result.coverage.rejected_accesses, 0U);
  EXPECT_EQ(result.coverage.emitted_line_references, 5U);
  EXPECT_EQ(result.l1.csrd_histogram, summary.l1.csrd_histogram);
  EXPECT_EQ(result.llc.csrd_histogram, summary.llc.csrd_histogram);
  expect_hierarchy_invariants(result.invariants);
}

TEST_F(HierarchyServiceSummaryTest, DerivesRatiosFromAuthoritativeCounts)
{
  summary.l1_first_hit_ratio = 99;
  summary.llc_first_hit_ratio = 99;
  summary.all_cache_miss_ratio = 99;
  const auto result = finalize_hierarchy_service(summary);
  ASSERT_TRUE(result.l1_first_hit_ratio);
  ASSERT_TRUE(result.llc_first_hit_ratio);
  ASSERT_TRUE(result.all_cache_miss_ratio);
  EXPECT_DOUBLE_EQ(*result.l1_first_hit_ratio, 0.2);
  EXPECT_DOUBLE_EQ(*result.llc_first_hit_ratio, 0.2);
  EXPECT_DOUBLE_EQ(*result.all_cache_miss_ratio, 0.6);
  expect_hierarchy_invariants(result.invariants);
}

TEST_F(HierarchyServiceSummaryTest, ClearsAllRatiosForZeroModeledAccesses)
{
  summary = {};
  summary.task_id = "empty";
  summary.l1_first_hit_ratio = 1;
  summary.llc_first_hit_ratio = 1;
  summary.all_cache_miss_ratio = 1;
  const auto result = finalize_hierarchy_service(summary);
  EXPECT_FALSE(result.l1_first_hit_ratio);
  EXPECT_FALSE(result.llc_first_hit_ratio);
  EXPECT_FALSE(result.all_cache_miss_ratio);
  expect_hierarchy_invariants(result.invariants);
}

TEST_F(HierarchyServiceSummaryTest, RejectsIncompleteCoverage)
{
  --summary.coverage.resolved_accesses;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsRejectedAccesses)
{
  summary.coverage.rejected_accesses = 1;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsSourceCoverageMismatch)
{
  ++summary.source_accesses;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLineCoverageMismatch)
{
  ++summary.coverage.emitted_line_references;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsModeledLookupMismatch)
{
  ++summary.modeled_accesses;
  ++summary.coverage.emitted_line_references;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsL1LookupConservationFailure)
{
  ++summary.l1.hits;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLlcLookupConservationFailure)
{
  ++summary.llc.hits;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsL1MissConservationFailure)
{
  --summary.l1.cold_misses;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLlcMissConservationFailure)
{
  --summary.llc.cold_misses;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLlcInputDisagreeingWithL1Misses)
{
  --summary.llc.lookups;
  --summary.llc.hits;
  --summary.llc_first_hit_count;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsL1FirstServiceDisagreeingWithHits)
{
  ++summary.l1_first_hit_count;
  --summary.llc_first_hit_count;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLlcFirstServiceDisagreeingWithHits)
{
  ++summary.llc_first_hit_count;
  --summary.all_cache_misses;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsMemoryServiceDisagreeingWithMisses)
{
  ++summary.all_cache_misses;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsInvalidCountsEvenWithPassedFlags)
{
  summary.invariants = {true, true, true, true, true};
  ++summary.all_cache_misses;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::logic_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsL1MissSumOverflow)
{
  summary.l1.cold_misses = maximum;
  summary.l1.replacement_misses = 1;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::overflow_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLlcMissSumOverflow)
{
  summary.llc.cold_misses = maximum;
  summary.llc.replacement_misses = 1;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::overflow_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsL1LookupSumOverflow)
{
  summary.l1.hits = maximum;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::overflow_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLlcLookupSumOverflow)
{
  summary.llc.hits = maximum;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::overflow_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsFirstServiceSumOverflow)
{
  summary.l1_first_hit_count = maximum;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::overflow_error);
}

TEST_F(HierarchyServiceSummaryTest, RejectsLlcServiceSumOverflow)
{
  summary.llc_first_hit_count = maximum;
  EXPECT_THROW(finalize_hierarchy_service(summary), std::overflow_error);
}

TEST_F(HierarchyServiceSummaryTest, AcceptsLargestRepresentableTotal)
{
  summary.source_accesses = maximum;
  summary.modeled_accesses = maximum;
  summary.coverage = {maximum, maximum, 0, maximum};
  summary.l1 = {maximum, maximum - 1, 1, 1, 0, 1, {{0, maximum - 1}}};
  summary.llc = {1, 0, 1, 1, 0, 1, {}};
  summary.l1_first_hit_count = maximum - 1;
  summary.llc_first_hit_count = 0;
  summary.all_cache_misses = 1;
  const auto result = finalize_hierarchy_service(summary);
  EXPECT_EQ(result.modeled_accesses, maximum);
  EXPECT_EQ(result.l1_first_hit_count, maximum - 1);
  EXPECT_EQ(result.all_cache_misses, 1U);
  ASSERT_TRUE(result.l1_first_hit_ratio);
  ASSERT_TRUE(result.llc_first_hit_ratio);
  ASSERT_TRUE(result.all_cache_miss_ratio);
  EXPECT_DOUBLE_EQ(*result.l1_first_hit_ratio, 1.0);
  EXPECT_DOUBLE_EQ(*result.llc_first_hit_ratio, 0.0);
  EXPECT_DOUBLE_EQ(*result.all_cache_miss_ratio,
                   1.0 / static_cast<double>(maximum));
  expect_hierarchy_invariants(result.invariants);
}

}  // namespace
