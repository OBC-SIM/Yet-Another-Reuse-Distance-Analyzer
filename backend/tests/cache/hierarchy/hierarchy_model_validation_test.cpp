#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "hierarchy_model_test_support.hpp"
#include "yarda/cache/hierarchy_model.hpp"
#include "yarda/cache/yaml_config_parser.hpp"

namespace
{

using yarda::select_analysis_hierarchy;
using yarda::test::support::expect_unsupported_hierarchy;
using yarda::test::support::make_analysis_config;
using yarda::test::support::make_multicore_analysis_config;

TEST(AnalysisHierarchyValidationTest, RejectsMissingCoreZeroMapping)
{
  auto config = make_multicore_analysis_config();
  config.core_mappings.pop_back();

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST(AnalysisHierarchyValidationTest, RejectsNonL1EntryRole)
{
  auto config = make_analysis_config();
  config.caches.front().role = "LLC";

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST(AnalysisHierarchyValidationTest, RejectsSharedEntryCache)
{
  auto config = make_analysis_config();
  config.caches.front().private_to.reset();

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST(AnalysisHierarchyValidationTest, RejectsEntryOwnedByAnotherCore)
{
  auto config = make_multicore_analysis_config();
  config.caches.front().private_to = 1;

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST(AnalysisHierarchyValidationTest, RejectsL1ConnectedDirectlyToMemory)
{
  auto config = make_analysis_config();
  config.caches.front().next = config.memory.name;

  expect_unsupported_hierarchy(config, "L1D0", "L1 next must reference an LLC");
}

TEST(AnalysisHierarchyValidationTest, RejectsNextCacheWithoutLlcRole)
{
  auto config = make_analysis_config();
  config.caches.back().role = "L2";

  expect_unsupported_hierarchy(config, "L2", "requires role=LLC");
}

TEST(AnalysisHierarchyValidationTest, RejectsPrivateLlc)
{
  auto config = make_analysis_config();
  config.caches.back().private_to = 0;

  expect_unsupported_hierarchy(config, "L2", "private_to");
}

TEST(AnalysisHierarchyValidationTest, RejectsInterveningCacheBeforeLlc)
{
  auto config = make_analysis_config();
  auto middle = config.caches.back();
  middle.name = "Middle";
  middle.role = "L2";
  middle.next = "L2";
  config.caches.front().next = middle.name;
  config.caches.push_back(middle);

  expect_unsupported_hierarchy(config, "Middle", "requires role=LLC");
}

TEST(AnalysisHierarchyValidationTest, RejectsExtraCacheAfterLlc)
{
  auto config = make_analysis_config();
  config.memory.name = "DRAM";
  auto extra = config.caches.back();
  extra.name = "L3";
  extra.next = config.memory.name;
  config.caches.back().next = extra.name;
  config.caches.push_back(extra);

  expect_unsupported_hierarchy(config, "L2", "DRAM");
}

TEST(AnalysisHierarchyValidationTest, RejectsUnequalLineSizes)
{
  auto config = make_analysis_config();
  config.caches.back().line_size = 64;

  expect_unsupported_hierarchy(config, "L2", "line_size");
}

class AnalysisCacheValidationTest : public testing::TestWithParam<std::size_t>
{
};

TEST_P(AnalysisCacheValidationTest, RejectsFifoReplacement)
{
  auto config = make_analysis_config();
  auto & cache = config.caches[GetParam()];
  cache.replacement = yarda::Replacement::FIFO;

  expect_unsupported_hierarchy(config, cache.name, "LRU");
}

TEST_P(AnalysisCacheValidationTest, RejectsMruReplacement)
{
  auto config = make_analysis_config();
  auto & cache = config.caches[GetParam()];
  cache.replacement = yarda::Replacement::MRU;

  expect_unsupported_hierarchy(config, cache.name, "LRU");
}

TEST_P(AnalysisCacheValidationTest, RejectsDisabledWriteAllocation)
{
  auto config = make_analysis_config();
  auto & cache = config.caches[GetParam()];
  cache.write_allocate = false;

  expect_unsupported_hierarchy(config, cache.name, "write_allocate");
}

TEST_P(AnalysisCacheValidationTest, RejectsZeroCapacity)
{
  auto config = make_analysis_config();
  config.caches[GetParam()].size_bytes = 0;

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST_P(AnalysisCacheValidationTest, RejectsNonPowerOfTwoGeometry)
{
  auto config = make_analysis_config();
  config.caches[GetParam()].associativity = 3;

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(SelectedLevels, AnalysisCacheValidationTest,
                         testing::Values(0U, 1U));

TEST(AnalysisHierarchyValidationTest, RejectsInvalidSchemaBeforeSelection)
{
  auto config = make_analysis_config();
  config.schema_version = 2;
  config.caches.back().replacement = yarda::Replacement::FIFO;

  try
  {
    select_analysis_hierarchy(config);
    ADD_FAILURE() << "expected invalid schema to be rejected before selection";
  }
  catch (const std::invalid_argument & error)
  {
    const std::string message = error.what();
    EXPECT_NE(message.find("schema version"), std::string::npos) << message;
  }
}

TEST(AnalysisHierarchyValidationTest, RejectsInvalidUnselectedGeometry)
{
  auto config = make_multicore_analysis_config();
  config.caches.back().size_bytes = 0;

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST(AnalysisHierarchyValidationTest, RejectsUnselectedCycle)
{
  auto config = make_multicore_analysis_config();
  config.caches.back().next = "L1D1";

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST(AnalysisHierarchyValidationTest, RejectsUnselectedBrokenReference)
{
  auto config = make_multicore_analysis_config();
  config.caches.back().next = "Missing";

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

TEST(AnalysisHierarchyValidationTest, RejectsGenericYamlPolicyExample)
{
  const auto config =
    yarda::parse_cache_config(YARDA_CACHE_CONFIG_EXAMPLE_PATH);
  ASSERT_NO_THROW(yarda::validate_cache_config(config));

  EXPECT_THROW(select_analysis_hierarchy(config), std::invalid_argument);
}

}  // namespace
