#pragma once

#include <optional>
#include <vector>

#include "yarda/cache/hierarchy_model.hpp"
#include "yarda/cache/hierarchy_summary.hpp"
#include "yarda/cache/lru_rd_analysis.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace yarda
{

/** @brief Identify the first level servicing one L1 line reference. */
enum class FirstServiceLevel
{
  L1,
  LLC,
  Memory,
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
 * @brief Own aligned cache observations for one L1 reference.
 *
 * Both LLC fields are present exactly for L1 misses. Mappings preserve the
 * same source/span provenance, with geometry-specific set/tag decoding.
 * The enclosing task summary supplies task_id; no borrowed data is retained.
 */
struct HierarchyAccessEvent
{
  CacheLineMapping l1_mapping;
  LruAccessResult l1;
  std::optional<CacheLineMapping> llc_mapping;
  std::optional<LruAccessResult> llc;
  FirstServiceLevel first_service = FirstServiceLevel::Memory;
};

/** @brief Own a checked task summary and one event per ordered L1 reference. */
struct BatchTaskHierarchyResult
{
  TaskHierarchySummary summary;
  std::vector<HierarchyAccessEvent> events;
};

/** @brief Own a complete batch analysis in input task order. */
struct BatchHierarchyResult
{
  std::vector<BatchTaskHierarchyResult> tasks;
  TraceCoverage coverage;
};

/**
 * @brief Analyze exact L1/LLC CSRD and first service for each cold task.
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
 * @throws std::logic_error if sizes, cross-level provenance, coverage or
 * level/service conservation disagree.
 */
BatchHierarchyResult
analyze_batch_hierarchy(const ResolvedTaskTraceResult & resolved,
                        const AnalysisHierarchy & hierarchy);

}  // namespace yarda
