#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

#include "exact_csrd_oracle.hpp"
#include "hierarchy_lru_oracle.hpp"
#include "hierarchy_lru_oracle_test_support.hpp"

namespace
{

using yarda::test::support::expect_contract_invariants;
using yarda::test::support::make_task;

std::size_t count_outcome(
  const std::vector<yarda::test::OracleCsrdObservation> & observations,
  yarda::LruAccessOutcome outcome)
{
  std::size_t count = 0;
  for (const auto & observation : observations)
  {
    if (observation.outcome == outcome)
    {
      ++count;
    }
  }
  return count;
}

TEST(ExplicitHierarchyLruOracleContractTest, PreservesEmptyTaskIdentity)
{
  const yarda::CacheGeometry geometry{32, 1, 1};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({}, geometry, "empty"), geometry, geometry);

  EXPECT_EQ(result.task_id, "empty");
  EXPECT_TRUE(result.events.empty());
  EXPECT_EQ(result.l1.lookups, 0U);
  EXPECT_EQ(result.l1.hits, 0U);
  EXPECT_EQ(result.l1.misses, 0U);
  EXPECT_EQ(result.l1.cold_misses, 0U);
  EXPECT_EQ(result.l1.replacement_misses, 0U);
  EXPECT_EQ(result.llc.lookups, 0U);
  EXPECT_EQ(result.llc.hits, 0U);
  EXPECT_EQ(result.llc.misses, 0U);
  EXPECT_EQ(result.llc.cold_misses, 0U);
  EXPECT_EQ(result.llc.replacement_misses, 0U);
  EXPECT_EQ(result.ehc_l1, 0U);
  EXPECT_EQ(result.ehc_llc, 0U);
  EXPECT_EQ(result.all_cache_misses, 0U);
  expect_contract_invariants(result, 0U);
}

TEST(ExplicitHierarchyLruOracleContractTest, CountsFirstServiceLevels)
{
  const yarda::CacheGeometry l1{32, 1, 1};
  const yarda::CacheGeometry llc{32, 2, 2};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({0, 1, 0, 0}, l1), l1, llc);

  EXPECT_EQ(result.ehc_l1, 1U);
  EXPECT_EQ(result.ehc_llc, 1U);
  EXPECT_EQ(result.all_cache_misses, 2U);
  EXPECT_EQ(result.l1.lookups, 4U);
  EXPECT_EQ(result.l1.hits, 1U);
  EXPECT_EQ(result.l1.misses, 3U);
  EXPECT_EQ(result.l1.cold_misses, 2U);
  EXPECT_EQ(result.l1.replacement_misses, 1U);
  EXPECT_EQ(result.llc.lookups, 3U);
  EXPECT_EQ(result.llc.hits, 1U);
  EXPECT_EQ(result.llc.misses, 2U);
  EXPECT_EQ(result.llc.cold_misses, 2U);
  EXPECT_EQ(result.llc.replacement_misses, 0U);
  expect_contract_invariants(result, 4U);
}

TEST(ExplicitHierarchyLruOracleContractTest,
     MatchesExactOraclesForContractDerivedStreams)
{
  const yarda::CacheGeometry l1{32, 2, 2};
  const yarda::CacheGeometry llc{32, 4, 4};
  const auto accesses = make_task({0, 1, 2, 0, 2}, l1);

  const auto hierarchy =
    yarda::test::analyze_with_explicit_lru(accesses, l1, llc);
  const auto l1_exact = yarda::test::naive_exact_csrd(accesses.accesses, l1);

  ASSERT_EQ(hierarchy.events.size(), 5U);
  ASSERT_EQ(l1_exact.size(), 5U);
  for (std::size_t index = 0; index < l1_exact.size(); ++index)
  {
    EXPECT_EQ(hierarchy.events[index].l1_outcome, l1_exact[index].outcome);
  }

  // The contract-derived L1 misses are references 0..3. Build the LLC stream
  // from those fixed indices instead of deriving it from hierarchy output.
  std::vector<yarda::CacheLineMapping> llc_accesses;
  for (std::size_t index = 0; index < 4; ++index)
  {
    auto remapped = accesses.accesses[index];
    remapped.decoded =
      yarda::decode_cache_address(remapped.decoded.address, llc);
    llc_accesses.push_back(remapped);
  }
  const auto llc_exact = yarda::test::naive_exact_csrd(llc_accesses, llc);

  ASSERT_EQ(llc_exact.size(), 4U);
  EXPECT_EQ(hierarchy.llc.lookups, 4U);
  for (std::size_t index = 0; index < llc_exact.size(); ++index)
  {
    ASSERT_TRUE(hierarchy.events[index].llc_outcome.has_value());
    EXPECT_EQ(*hierarchy.events[index].llc_outcome, llc_exact[index].outcome);
  }
  EXPECT_FALSE(hierarchy.events[4].llc_outcome.has_value());

  EXPECT_EQ(hierarchy.l1.hits,
            count_outcome(l1_exact, yarda::LruAccessOutcome::Hit));
  EXPECT_EQ(hierarchy.l1.cold_misses,
            count_outcome(l1_exact, yarda::LruAccessOutcome::ColdMiss));
  EXPECT_EQ(hierarchy.l1.replacement_misses,
            count_outcome(l1_exact, yarda::LruAccessOutcome::ReplacementMiss));
  EXPECT_EQ(hierarchy.llc.hits,
            count_outcome(llc_exact, yarda::LruAccessOutcome::Hit));
  EXPECT_EQ(hierarchy.llc.cold_misses,
            count_outcome(llc_exact, yarda::LruAccessOutcome::ColdMiss));
  EXPECT_EQ(hierarchy.llc.replacement_misses,
            count_outcome(llc_exact, yarda::LruAccessOutcome::ReplacementMiss));
  expect_contract_invariants(hierarchy, 5U);
}

TEST(ExplicitHierarchyLruOracleContractTest, RejectsUnequalLineSizes)
{
  const yarda::CacheGeometry l1{32, 4, 2};
  const yarda::CacheGeometry llc{64, 4, 2};

  EXPECT_THROW(
    yarda::test::analyze_with_explicit_lru(make_task({1}, l1), l1, llc),
    std::invalid_argument);
}

TEST(ExplicitHierarchyLruOracleContractTest, RejectsInconsistentBlockNumber)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = make_task({0}, geometry);
  ASSERT_EQ(accesses.accesses.size(), 1U);
  accesses.accesses[0].decoded.block_number += yarda::cache_set_count(geometry);
  ++accesses.accesses[0].decoded.tag;

  EXPECT_THROW(
    yarda::test::analyze_with_explicit_lru(accesses, geometry, geometry),
    std::invalid_argument);
}

TEST(ExplicitHierarchyLruOracleContractTest, RejectsInconsistentSetIndex)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = make_task({0}, geometry);
  ASSERT_EQ(accesses.accesses.size(), 1U);
  ++accesses.accesses[0].decoded.set_index;

  EXPECT_THROW(
    yarda::test::analyze_with_explicit_lru(accesses, geometry, geometry),
    std::invalid_argument);
}

TEST(ExplicitHierarchyLruOracleContractTest, RejectsInconsistentTag)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = make_task({0}, geometry);
  ASSERT_EQ(accesses.accesses.size(), 1U);
  ++accesses.accesses[0].decoded.tag;

  EXPECT_THROW(
    yarda::test::analyze_with_explicit_lru(accesses, geometry, geometry),
    std::invalid_argument);
}

TEST(ExplicitHierarchyLruOracleContractTest, RejectsInconsistentLineOffset)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = make_task({0}, geometry);
  ASSERT_EQ(accesses.accesses.size(), 1U);
  ++accesses.accesses[0].decoded.line_offset;

  EXPECT_THROW(
    yarda::test::analyze_with_explicit_lru(accesses, geometry, geometry),
    std::invalid_argument);
}

TEST(ExplicitHierarchyLruOracleContractTest,
     RejectsMappingDecodedForAnotherL1Geometry)
{
  const yarda::CacheGeometry source_geometry{32, 2, 1};
  const yarda::CacheGeometry supplied_geometry{32, 4, 1};

  EXPECT_THROW(
    yarda::test::analyze_with_explicit_lru(
      make_task({2}, source_geometry), supplied_geometry, supplied_geometry),
    std::invalid_argument);
}

TEST(ExplicitHierarchyLruOracleContractTest, RejectsInvalidGeometry)
{
  const yarda::CacheGeometry invalid_geometry{0, 4, 2};

  // Empty input ensures the fixture does not decode the invalid geometry.
  EXPECT_THROW(
    yarda::test::analyze_with_explicit_lru(make_task({}, invalid_geometry),
                                           invalid_geometry, invalid_geometry),
    std::invalid_argument);
}

}  // namespace
