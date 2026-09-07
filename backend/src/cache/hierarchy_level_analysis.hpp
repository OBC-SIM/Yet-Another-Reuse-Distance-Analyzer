#pragma once

#include "yarda/cache/hierarchy_analysis.hpp"

namespace yarda::detail
{

/**
 * @brief Convert a batch primitive result to checked semantic level counts.
 *
 * @param analysis Decisions and counts from the batch LRU primitive.
 * @param mappings Ordered input of the primitive.
 * @return Summary with independently counted unique linked blocks.
 * @throws std::overflow_error if a size, distance or count exceeds uint64_t.
 * @throws std::logic_error if sizes, counts, histogram or unique lines
 * disagree.
 */
CacheLevelSummary
summarize_hierarchy_level(const LruRdAnalysis & analysis,
                          const std::vector<CacheLineMapping> & mappings);

/**
 * @brief Analyze one cold level and retain its ordered validation payload.
 *
 * @param mappings Mappings transferred into the returned result.
 * @param geometry Geometry used by the mappings.
 * @return Owned summary, mappings and aligned LRU decisions.
 * @throws std::invalid_argument if the batch primitive rejects the input.
 * @throws std::overflow_error if summary arithmetic overflows.
 * @throws std::logic_error if the primitive produces inconsistent results.
 */
BatchCacheLevelResult analyze_hierarchy_level(
  std::vector<CacheLineMapping> mappings, const CacheGeometry & geometry);

}  // namespace yarda::detail
