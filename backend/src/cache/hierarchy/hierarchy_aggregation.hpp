#pragma once

#include "yarda/cache/hierarchy_analysis.hpp"

namespace yarda::detail
{

/**
 * @brief Pair ordered L1 misses with LLC rows and finalize one task.
 *
 * @param task_id Owning identity of both streams; scopes their row ordinals.
 * @param coverage Complete source-to-L1 coverage from the mapped task.
 * @param l1 Owned L1 stream; mappings are transferred to returned events.
 * @param llc Owned miss-only stream belonging to the same task as l1.
 * @pre Both level summaries were validated by analyze_hierarchy_level().
 * @return Owned summary and events, with no partial result on failure.
 * @throws std::logic_error for misaligned streams, different provenance,
 * missing/leftover LLC rows, or inconsistent coverage and service counts.
 * @throws std::overflow_error if service or conservation arithmetic overflows.
 */
BatchTaskHierarchyResult
aggregate_hierarchy_task(std::string task_id, const TraceCoverage & coverage,
                         BatchCacheLevelResult l1, BatchCacheLevelResult llc);

}  // namespace yarda::detail
