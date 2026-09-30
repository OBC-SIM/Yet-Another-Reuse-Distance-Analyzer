#include "yarda/cache/hierarchy_model.hpp"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>

#include "hierarchy_model_test_support.hpp"
#include "yarda/cache/yaml_config_parser.hpp"

namespace
{

using yarda::select_analysis_hierarchy;
using yarda::test::support::expect_analysis_hierarchy;
using yarda::test::support::make_analysis_config;
using yarda::test::support::make_multicore_analysis_config;

TEST(AnalysisHierarchyTest, Selects32ByteHierarchyWithIndependentGeometries)
{
  const auto hierarchy = select_analysis_hierarchy(make_analysis_config());

  expect_analysis_hierarchy(hierarchy, {0,
                                        {"L1D0", "L1", {32, 1024, 4}},
                                        {"L2", "LLC", {32, 65536, 8}},
                                        "Memory"});
}

TEST(AnalysisHierarchyTest, Accepts64ByteLinesWithoutChangingCapacity)
{
  const auto hierarchy = select_analysis_hierarchy(make_analysis_config(64));

  expect_analysis_hierarchy(
    hierarchy,
    {0, {"L1D0", "L1", {64, 512, 4}}, {"L2", "LLC", {64, 32768, 8}}, "Memory"});
}

TEST(AnalysisHierarchyTest, AcceptsShipped32ByteYaml)
{
  const auto config = yarda::parse_cache_config(YARDA_CACHE_CONFIG_32B_PATH);

  expect_analysis_hierarchy(select_analysis_hierarchy(config),
                            {0,
                             {"L1D0", "L1", {32, 1024, 4}},
                             {"L2", "LLC", {32, 65536, 4}},
                             "Memory"});
}

TEST(AnalysisHierarchyTest, FollowsConfiguredNamesRegardlessOfCacheOrder)
{
  auto config = make_analysis_config();
  config.core_mappings.front().l1 = "Data";
  config.caches.front().name = "Data";
  config.caches.front().next = "SharedLast";
  config.caches.back().name = "SharedLast";
  config.caches.back().next = "DRAM";
  config.memory.name = "DRAM";
  std::reverse(config.caches.begin(), config.caches.end());

  expect_analysis_hierarchy(select_analysis_hierarchy(config),
                            {0,
                             {"Data", "L1", {32, 1024, 4}},
                             {"SharedLast", "LLC", {32, 65536, 8}},
                             "DRAM"});
}

TEST(AnalysisHierarchyTest, SelectsCoreZeroAndIgnoresOtherValidCachePolicies)
{
  auto config = make_multicore_analysis_config();
  std::reverse(config.caches.begin(), config.caches.end());
  ASSERT_NO_THROW(yarda::validate_cache_config(config));

  expect_analysis_hierarchy(select_analysis_hierarchy(config),
                            {0,
                             {"L1D0", "L1", {32, 1024, 4}},
                             {"L2", "LLC", {32, 65536, 8}},
                             "Memory"});
}

TEST(AnalysisHierarchyTest, AcceptsLlcSharedByTwoCores)
{
  auto config = make_analysis_config();
  config.num_cores = 2;
  config.core_mappings.push_back({1, "L1D1"});
  auto second_l1 = config.caches.front();
  second_l1.name = "L1D1";
  second_l1.private_to = 1;
  config.caches.push_back(second_l1);

  expect_analysis_hierarchy(select_analysis_hierarchy(config),
                            {0,
                             {"L1D0", "L1", {32, 1024, 4}},
                             {"L2", "LLC", {32, 65536, 8}},
                             "Memory"});
}

TEST(AnalysisHierarchyTest, OwnsSnapshotAfterSourceConfigurationChanges)
{
  auto config = make_analysis_config();
  const auto hierarchy = select_analysis_hierarchy(config);
  config.caches.front().name = "ChangedL1";
  config.caches.front().role = "ChangedRole";
  config.caches.front().line_size = 64;
  config.caches.front().size_bytes = 64 * 1024;
  config.caches.front().associativity = 16;
  config.caches.back().name = "ChangedLLC";
  config.caches.back().role = "AnotherRole";
  config.caches.back().line_size = 128;
  config.caches.back().size_bytes = 4 * 1024 * 1024;
  config.caches.back().associativity = 32;
  config.memory.name = "ChangedMemory";

  expect_analysis_hierarchy(hierarchy, {0,
                                        {"L1D0", "L1", {32, 1024, 4}},
                                        {"L2", "LLC", {32, 65536, 8}},
                                        "Memory"});
}

TEST(AnalysisHierarchyTest, OwnsSnapshotAfterSourceConfigurationIsDestroyed)
{
  yarda::AnalysisHierarchy hierarchy;
  {
    const auto config = make_analysis_config();
    hierarchy = select_analysis_hierarchy(config);
  }

  expect_analysis_hierarchy(hierarchy, {0,
                                        {"L1D0", "L1", {32, 1024, 4}},
                                        {"L2", "LLC", {32, 65536, 8}},
                                        "Memory"});
}

class AnalysisCacheIgnoredFieldsTest
  : public testing::TestWithParam<std::size_t>
{
};

TEST_P(AnalysisCacheIgnoredFieldsTest, IgnoresWritePolicy)
{
  auto config = make_analysis_config();
  const auto baseline = select_analysis_hierarchy(config);
  config.caches[GetParam()].write_policy = yarda::WritePolicy::WriteThrough;

  expect_analysis_hierarchy(select_analysis_hierarchy(config), baseline);
}

TEST_P(AnalysisCacheIgnoredFieldsTest, IgnoresDelayCycles)
{
  auto config = make_analysis_config();
  const auto baseline = select_analysis_hierarchy(config);
  config.caches[GetParam()].delay_cycles = 0;

  expect_analysis_hierarchy(select_analysis_hierarchy(config), baseline);
}

INSTANTIATE_TEST_SUITE_P(SelectedLevels, AnalysisCacheIgnoredFieldsTest,
                         testing::Values(0U, 1U));

TEST(AnalysisHierarchyTest, IgnoresMemoryDelayCycles)
{
  auto config = make_analysis_config();
  const auto baseline = select_analysis_hierarchy(config);
  config.memory.delay_cycles = 0;

  expect_analysis_hierarchy(select_analysis_hierarchy(config), baseline);
}

}  // namespace
