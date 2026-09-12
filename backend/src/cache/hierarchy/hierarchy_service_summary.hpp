#pragma once

#include "yarda/cache/hierarchy_summary.hpp"

namespace yarda::detail
{

/**
 * @brief Add service counters without wrapping.
 *
 * @param left Accumulated count.
 * @param right Additional count.
 * @return Exact sum.
 * @throws std::overflow_error if the sum exceeds uint64_t.
 */
std::uint64_t checked_service_sum(std::uint64_t left, std::uint64_t right);

/**
 * @brief Check task conservation and derive first-service ratios.
 *
 * @param summary Owned counts and source-to-L1 coverage to finalize; existing
 * ratios and invariant flags are overwritten from the authoritative counts.
 * @pre Level histograms and unique counts were validated during level analysis.
 * @return Complete semantic summary, owning no per-reference data.
 * @throws std::logic_error for inconsistent coverage or conservation.
 * @throws std::overflow_error if any checked conservation sum overflows.
 */
TaskHierarchySummary finalize_hierarchy_service(TaskHierarchySummary summary);

}  // namespace yarda::detail
