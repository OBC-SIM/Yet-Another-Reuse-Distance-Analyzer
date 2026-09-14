#pragma once

#include <cstdint>
#include <string>

#include "yarda/cache/cache_config.hpp"

namespace yarda
{

/**
 * @brief Own the identity and geometry of one selected analysis cache.
 *
 * The selected model fixes replacement to LRU and allocation on every demand
 * miss. Configured write propagation and delay are not modeled here.
 */
struct AnalysisCacheLevel
{
  std::string name;
  std::string role;
  CacheGeometry geometry;
};

/**
 * @brief Own a snapshot of the selected core-0 analysis hierarchy.
 *
 * Names and geometries remain valid after the source configuration changes or
 * is destroyed. Obtain a supported snapshot with select_analysis_hierarchy().
 */
struct AnalysisHierarchy
{
  std::uint32_t core_id = 0;
  AnalysisCacheLevel l1;
  AnalysisCacheLevel llc;
  std::string memory_name;
};

/**
 * @brief Select core 0 for the exact-two-level-lru-demand-v1 model.
 *
 * Validates the entire configuration, then requires private L1 -> shared LLC
 * -> configured Memory, equal line sizes, LRU and write_allocate at both
 * levels. Valid caches outside this path need only satisfy general validation.
 * write_policy and cache/memory delay_cycles do not affect residency, CSRD or
 * first-service metrics and are omitted from the returned model.
 *
 * @param config Hierarchy to validate and select without modifying or owning
 * it.
 * @return Value-owned core-0 hierarchy with independent names and geometries.
 * @throws std::invalid_argument if the configuration is invalid or the selected
 * path is unsupported by this model.
 */
AnalysisHierarchy select_analysis_hierarchy(const HierarchyConfig & config);

}  // namespace yarda
