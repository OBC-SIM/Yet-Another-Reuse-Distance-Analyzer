#include <cstddef>
#include <gtest/gtest.h>

#include "hierarchy_lru_oracle.hpp"
#include "hierarchy_lru_oracle_test_support.hpp"

namespace
{

using yarda::test::support::expect_contract_invariants;
using yarda::test::support::make_mapping;
using yarda::test::support::make_task;

TEST(ExplicitHierarchyLruOracleOperationTest, AllocatesLlcForStoreMiss)
{
  const yarda::CacheGeometry l1{32, 1, 1};
  const yarda::CacheGeometry llc{32, 2, 2};
  auto accesses = make_task({}, l1, "store-llc");
  accesses.accesses.push_back(
    make_mapping(0, l1, 0, 0, yarda::AccessOperation::Store));
  accesses.accesses.push_back(
    make_mapping(1, l1, 1, 0, yarda::AccessOperation::Load));
  accesses.accesses.push_back(
    make_mapping(0, l1, 2, 0, yarda::AccessOperation::Load));

  const auto result = yarda::test::analyze_with_explicit_lru(accesses, l1, llc);

  ASSERT_EQ(result.events.size(), 3U);
  EXPECT_EQ(result.events[2].l1_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  ASSERT_TRUE(result.events[2].llc_outcome.has_value());
  EXPECT_EQ(*result.events[2].llc_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.events[2].first_service,
            yarda::test::OracleFirstServiceLevel::LLC);
  expect_contract_invariants(result, 3U);
}

TEST(ExplicitHierarchyLruOracleOperationTest,
     TreatsLoadAndStoreIdenticallyAcrossRecencyAndReplacement)
{
  const yarda::CacheGeometry geometry{32, 2, 2};
  auto loads = make_task({0, 1, 0, 2, 0, 1}, geometry, "all-loads");
  auto stores = make_task({0, 1, 0, 2, 0, 1}, geometry, "all-stores");
  for (const auto & mapping : loads.accesses)
  {
    ASSERT_EQ(mapping.operation, yarda::AccessOperation::Load);
  }
  for (auto & mapping : stores.accesses)
  {
    mapping.operation = yarda::AccessOperation::Store;
  }

  const auto load_result =
    yarda::test::analyze_with_explicit_lru(loads, geometry, geometry);
  const auto store_result =
    yarda::test::analyze_with_explicit_lru(stores, geometry, geometry);

  ASSERT_EQ(load_result.events.size(), 6U);
  ASSERT_EQ(store_result.events.size(), 6U);
  EXPECT_EQ(store_result.events[4].l1_outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(store_result.events[5].l1_outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  for (std::size_t index = 0; index < store_result.events.size(); ++index)
  {
    EXPECT_EQ(store_result.events[index].l1_outcome,
              load_result.events[index].l1_outcome);
    EXPECT_EQ(store_result.events[index].llc_outcome,
              load_result.events[index].llc_outcome);
    EXPECT_EQ(store_result.events[index].first_service,
              load_result.events[index].first_service);
  }
  EXPECT_EQ(store_result.l1.hits, load_result.l1.hits);
  EXPECT_EQ(store_result.l1.cold_misses, load_result.l1.cold_misses);
  EXPECT_EQ(store_result.l1.replacement_misses,
            load_result.l1.replacement_misses);
  EXPECT_EQ(store_result.llc.hits, load_result.llc.hits);
  EXPECT_EQ(store_result.llc.cold_misses, load_result.llc.cold_misses);
  EXPECT_EQ(store_result.llc.replacement_misses,
            load_result.llc.replacement_misses);
  expect_contract_invariants(load_result, 6U);
  expect_contract_invariants(store_result, 6U);
}

}  // namespace
