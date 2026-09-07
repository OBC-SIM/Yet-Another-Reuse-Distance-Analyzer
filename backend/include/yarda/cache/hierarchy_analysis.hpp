#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "yarda/cache/hierarchy_model.hpp"
#include "yarda/cache/lru_rd_analysis.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace yarda
{

/**
 * @brief Count one cold-started cache-level input stream.
 *
 * The accounting unit is a cache-line reference. Finite distances, including
 * those above associativity, remain separate histogram keys. Unique lines
 * count linked blocks independently of source object identity.
 */
struct CacheLevelSummary
{
  std::uint64_t lookups = 0;
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t cold_misses = 0;
  std::uint64_t replacement_misses = 0;
  std::uint64_t unique_lines = 0;
  std::map<std::uint64_t, std::uint64_t> csrd_histogram;
};

/**
 * @brief Own ordered mappings and aligned LRU decisions for batch validation.
 *
 * Each decision corresponds to the mapping at the same index. Summary data
 * owns no reference vectors and can be reused by a streaming consumer.
 */
struct BatchCacheLevelResult
{
  CacheLevelSummary summary;
  std::vector<CacheLineMapping> mappings;
  std::vector<LruAccessResult> accesses;
};

/**
 * @brief Own the two independent cold cache streams of one analyzed task.
 *
 * LLC rows contain only L1 misses, in their original order. Both levels retain
 * source and span ordinals scoped by task_id. Coverage counts L1 references;
 * source_accesses counts source ranges and modeled_accesses counts L1 rows.
 */
struct BatchTaskHierarchyResult
{
  std::string task_id;
  std::uint64_t source_accesses = 0;
  std::uint64_t modeled_accesses = 0;
  BatchCacheLevelResult l1;
  BatchCacheLevelResult llc;
  TraceCoverage coverage;
};

/** @brief Own a complete batch analysis in input task order. */
struct BatchHierarchyResult
{
  std::vector<BatchTaskHierarchyResult> tasks;
  TraceCoverage coverage;
};

/**
 * @brief Analyze full exact L1 and miss-filtered LLC CSRD for each task.
 *
 * Empty tasks are retained. Each task starts with cold state at both levels.
 * Load and store have identical residency semantics. LLC mappings decode the
 * original linked addresses at LLC geometry. Any invalid task fails the
 * entire call; no partial result is returned and inputs are not modified.
 *
 * @param resolved Borrowed resolved tasks with complete, consistent coverage,
 * no opaque-call exclusions, absolute addresses, load/store operations and
 * consecutive task-local source ordinals starting at zero.
 * @param hierarchy Borrowed selected model snapshot.
 * @pre hierarchy is an unmodified result of select_analysis_hierarchy().
 * @return Owned task results and aggregate source-to-L1 coverage.
 * @throws std::invalid_argument for unsupported or inconsistent input, empty
 * or duplicate task IDs, no tasks, or invalid or unequal-line-size geometries.
 * @throws std::overflow_error for address, distance or counter overflow.
 * @throws std::logic_error if level result sizes or conservation disagree.
 */
BatchHierarchyResult
analyze_batch_hierarchy(const ResolvedTaskTraceResult & resolved,
                        const AnalysisHierarchy & hierarchy);

}  // namespace yarda
