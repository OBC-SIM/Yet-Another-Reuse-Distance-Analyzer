#include "hierarchy_lru_oracle.hpp"

#include <gtest/gtest.h>

#include "hierarchy_lru_oracle_test_support.hpp"

namespace
{

using yarda::test::support::expect_contract_invariants;
using yarda::test::support::make_mapping;
using yarda::test::support::make_task;

TEST(ExplicitHierarchyLruOracleTest, SkipsLlcLookupOnL1Hit)
{
  const yarda::CacheGeometry l1{32, 1, 1};
  const yarda::CacheGeometry llc{32, 2, 2};

  const auto result =
    yarda::test::analyze_with_explicit_lru(make_task({0, 0}, l1), l1, llc);

  ASSERT_EQ(result.events.size(), 2U);
  EXPECT_EQ(result.events[1].l1_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_FALSE(result.events[1].llc_outcome.has_value());
  EXPECT_EQ(result.events[1].first_service,
            yarda::test::OracleFirstServiceLevel::L1);
  EXPECT_EQ(result.llc.lookups, 1U);
  expect_contract_invariants(result, 2U);
}

TEST(ExplicitHierarchyLruOracleTest, FillsL1AfterLlcHit)
{
  const yarda::CacheGeometry l1{32, 1, 1};
  const yarda::CacheGeometry llc{32, 2, 2};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({0, 1, 0, 0}, l1), l1, llc);

  ASSERT_EQ(result.events.size(), 4U);
  ASSERT_TRUE(result.events[2].llc_outcome.has_value());
  EXPECT_EQ(*result.events[2].llc_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.events[2].first_service,
            yarda::test::OracleFirstServiceLevel::LLC);
  EXPECT_EQ(result.events[3].l1_outcome, yarda::LruAccessOutcome::Hit);
  expect_contract_invariants(result, 4U);
}

TEST(ExplicitHierarchyLruOracleTest, FillsBothLevelsAfterMemoryService)
{
  const yarda::CacheGeometry l1{32, 1, 1};
  const yarda::CacheGeometry llc{32, 2, 2};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({0, 0, 1, 0}, l1), l1, llc);

  ASSERT_EQ(result.events.size(), 4U);
  EXPECT_EQ(result.events[0].l1_outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result.events[0].llc_outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result.events[0].first_service,
            yarda::test::OracleFirstServiceLevel::Memory);
  EXPECT_EQ(result.events[1].l1_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.events[3].l1_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  ASSERT_TRUE(result.events[3].llc_outcome.has_value());
  EXPECT_EQ(*result.events[3].llc_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.events[3].first_service,
            yarda::test::OracleFirstServiceLevel::LLC);
  // Independent levels make fill order unobservable; both resulting fills are
  // observed here after the first memory service.
  expect_contract_invariants(result, 4U);
}

TEST(ExplicitHierarchyLruOracleTest, DoesNotInsertL1VictimIntoLlc)
{
  const yarda::CacheGeometry l1{32, 4, 4};
  const yarda::CacheGeometry llc{32, 2, 2};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({0, 1, 2, 3, 4, 0}, l1), l1, llc);

  ASSERT_EQ(result.events.size(), 6U);
  EXPECT_EQ(result.events.back().l1_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  ASSERT_TRUE(result.events.back().llc_outcome.has_value());
  EXPECT_EQ(*result.events.back().llc_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(result.events.back().first_service,
            yarda::test::OracleFirstServiceLevel::Memory);
  expect_contract_invariants(result, 6U);
}

TEST(ExplicitHierarchyLruOracleTest, DoesNotBackInvalidateL1OnLlcEviction)
{
  const yarda::CacheGeometry l1{32, 4, 2};
  const yarda::CacheGeometry llc{32, 2, 2};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({0, 1, 3, 0}, l1), l1, llc);

  ASSERT_EQ(result.events.size(), 4U);
  ASSERT_TRUE(result.events[2].llc_outcome.has_value());
  EXPECT_EQ(*result.events[2].llc_outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result.events.back().l1_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_FALSE(result.events.back().llc_outcome.has_value());
  expect_contract_invariants(result, 4U);
}

TEST(ExplicitHierarchyLruOracleTest, RemapsAddressForEachCacheGeometry)
{
  const yarda::CacheGeometry l1{32, 2, 1};
  const yarda::CacheGeometry llc{32, 8, 2};

  const auto result =
    yarda::test::analyze_with_explicit_lru(make_task({0, 2, 0}, l1), l1, llc);

  ASSERT_EQ(result.events.size(), 3U);
  ASSERT_TRUE(result.events[1].llc_outcome.has_value());
  EXPECT_EQ(*result.events[1].llc_outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result.events[1].first_service,
            yarda::test::OracleFirstServiceLevel::Memory);
  EXPECT_EQ(result.events[2].l1_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  ASSERT_TRUE(result.events[2].llc_outcome.has_value());
  EXPECT_EQ(*result.events[2].llc_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.events[2].first_service,
            yarda::test::OracleFirstServiceLevel::LLC);
  EXPECT_EQ(result.llc.replacement_misses, 0U);
  expect_contract_invariants(result, 3U);
}

TEST(ExplicitHierarchyLruOracleTest, PromotesL1HitLineToMostRecentlyUsed)
{
  const yarda::CacheGeometry geometry{32, 2, 2};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({0, 1, 0, 2, 0}, geometry), geometry, geometry);

  ASSERT_EQ(result.events.size(), 5U);
  EXPECT_EQ(result.events[2].l1_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.events[3].l1_outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result.events[4].l1_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_FALSE(result.events[4].llc_outcome.has_value());
  expect_contract_invariants(result, 5U);
}

TEST(ExplicitHierarchyLruOracleTest, PromotesLlcHitLineToMostRecentlyUsed)
{
  const yarda::CacheGeometry l1{32, 1, 1};
  const yarda::CacheGeometry llc{32, 2, 2};

  const auto result = yarda::test::analyze_with_explicit_lru(
    make_task({0, 1, 0, 2, 0}, l1), l1, llc);

  ASSERT_EQ(result.events.size(), 5U);
  ASSERT_TRUE(result.events[2].llc_outcome.has_value());
  EXPECT_EQ(*result.events[2].llc_outcome, yarda::LruAccessOutcome::Hit);
  ASSERT_TRUE(result.events[3].llc_outcome.has_value());
  EXPECT_EQ(*result.events[3].llc_outcome, yarda::LruAccessOutcome::ColdMiss);
  ASSERT_TRUE(result.events[4].llc_outcome.has_value());
  EXPECT_EQ(*result.events[4].llc_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.events[4].first_service,
            yarda::test::OracleFirstServiceLevel::LLC);
  expect_contract_invariants(result, 5U);
}

TEST(ExplicitHierarchyLruOracleTest, StartsEveryTaskFromColdState)
{
  const yarda::CacheGeometry geometry{32, 1, 1};

  const auto first = yarda::test::analyze_with_explicit_lru(
    make_task({0, 0}, geometry, "first"), geometry, geometry);
  const auto second = yarda::test::analyze_with_explicit_lru(
    make_task({0}, geometry, "second"), geometry, geometry);

  ASSERT_EQ(second.events.size(), 1U);
  EXPECT_EQ(second.task_id, "second");
  EXPECT_EQ(second.events[0].l1_outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(second.events[0].llc_outcome, yarda::LruAccessOutcome::ColdMiss);
  expect_contract_invariants(first, 2U);
  expect_contract_invariants(second, 1U);
}

TEST(ExplicitHierarchyLruOracleTest, TreatsOffsetsWithinOneLineAsOneBlock)
{
  const yarda::CacheGeometry geometry{32, 1, 1};
  auto accesses = make_task({}, geometry, "offsets");
  accesses.accesses.push_back(make_mapping(3, geometry, 0, 8));
  accesses.accesses.push_back(make_mapping(3, geometry, 1, 24));

  const auto result =
    yarda::test::analyze_with_explicit_lru(accesses, geometry, geometry);

  ASSERT_EQ(result.events.size(), 2U);
  EXPECT_EQ(result.events[0].l1_outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result.events[1].l1_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_FALSE(result.events[1].llc_outcome.has_value());
  expect_contract_invariants(result, 2U);
}

TEST(ExplicitHierarchyLruOracleTest, ClassifiesEvictedLineByBlockNotAddress)
{
  const yarda::CacheGeometry geometry{32, 1, 1};
  auto accesses = make_task({}, geometry, "offsets-miss");
  accesses.accesses.push_back(make_mapping(3, geometry, 0, 8));
  accesses.accesses.push_back(make_mapping(1, geometry, 1));
  accesses.accesses.push_back(make_mapping(3, geometry, 2, 24));

  const auto result =
    yarda::test::analyze_with_explicit_lru(accesses, geometry, geometry);

  ASSERT_EQ(result.events.size(), 3U);
  EXPECT_EQ(result.events[2].l1_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  ASSERT_TRUE(result.events[2].llc_outcome.has_value());
  EXPECT_EQ(*result.events[2].llc_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(result.events[2].first_service,
            yarda::test::OracleFirstServiceLevel::Memory);
  expect_contract_invariants(result, 3U);
}

}  // namespace
