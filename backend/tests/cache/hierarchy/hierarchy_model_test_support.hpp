#pragma once

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "yarda/cache/hierarchy_model.hpp"

namespace yarda::test::support
{

/**
 * @brief Build a supported hierarchy with different capacities and way counts.
 *
 * @param line_size Common line size in bytes, either 32 or 64 in these tests.
 * @return Independent configuration with an LRU, allocating L1 and LLC.
 */
inline HierarchyConfig make_analysis_config(std::uint64_t line_size = 32)
{
  HierarchyConfig config;
  config.schema_version = 1;
  config.num_cores = 1;
  config.core_mappings = {{0, "L1D0"}};
  config.caches = {
    {"L1D0", "L1", 0, 32 * 1024, line_size, 4, Replacement::LRU,
     WritePolicy::WriteBack, true, 1, "L2"},
    {"L2", "LLC", std::nullopt, 2 * 1024 * 1024, line_size, 8, Replacement::LRU,
     WritePolicy::WriteBack, true, 12, "Memory"},
  };
  config.memory = {"Memory", 120};
  return config;
}

/**
 * @brief Add an unselected core whose caches violate only analysis
 * restrictions.
 *
 * @return Valid general configuration with core 1 first in the mapping list.
 */
inline HierarchyConfig make_multicore_analysis_config()
{
  auto config = make_analysis_config();
  config.num_cores = 2;
  config.core_mappings.insert(config.core_mappings.begin(), {1, "L1D1"});
  config.caches.push_back({"L1D1", "L1", 1, 16 * 1024, 128, 2,
                           Replacement::FIFO, WritePolicy::WriteThrough, false,
                           3, "Other"});
  config.caches.push_back({"Other", "L2", std::nullopt, 64 * 1024, 64, 4,
                           Replacement::MRU, WritePolicy::WriteThrough, false,
                           8, "Memory"});
  return config;
}

/**
 * @brief Compare every modeled field without deriving expected geometry.
 *
 * @param actual Selected model under test.
 * @param expected Independently specified model or an unchanged baseline.
 * @return Nothing; reports any mismatch through Google Test.
 */
inline void expect_analysis_hierarchy(const AnalysisHierarchy & actual,
                                      const AnalysisHierarchy & expected)
{
  EXPECT_EQ(actual.core_id, expected.core_id);
  EXPECT_EQ(actual.l1.name, expected.l1.name);
  EXPECT_EQ(actual.l1.role, expected.l1.role);
  EXPECT_EQ(actual.l1.geometry.line_size, expected.l1.geometry.line_size);
  EXPECT_EQ(actual.l1.geometry.line_count, expected.l1.geometry.line_count);
  EXPECT_EQ(actual.l1.geometry.associativity,
            expected.l1.geometry.associativity);
  EXPECT_EQ(actual.llc.name, expected.llc.name);
  EXPECT_EQ(actual.llc.role, expected.llc.role);
  EXPECT_EQ(actual.llc.geometry.line_size, expected.llc.geometry.line_size);
  EXPECT_EQ(actual.llc.geometry.line_count, expected.llc.geometry.line_count);
  EXPECT_EQ(actual.llc.geometry.associativity,
            expected.llc.geometry.associativity);
  EXPECT_EQ(actual.memory_name, expected.memory_name);
}

/**
 * @brief Require a model-specific rejection after general validation succeeds.
 *
 * @param config Generally valid hierarchy with one unsupported path condition.
 * @param cache_name Cache that the diagnostic must identify.
 * @param condition Model requirement that the diagnostic must explain.
 * @return Nothing; reports missing rejection or diagnostic through Google Test.
 */
inline void expect_unsupported_hierarchy(const HierarchyConfig & config,
                                         const std::string & cache_name,
                                         const std::string & condition)
{
  ASSERT_NO_THROW(validate_cache_config(config));
  try
  {
    select_analysis_hierarchy(config);
    ADD_FAILURE() << "expected unsupported analysis hierarchy";
  }
  catch (const std::invalid_argument & error)
  {
    const std::string message = error.what();
    EXPECT_NE(message.find(cache_name), std::string::npos) << message;
    EXPECT_NE(message.find(condition), std::string::npos) << message;
  }
}

}  // namespace yarda::test::support
