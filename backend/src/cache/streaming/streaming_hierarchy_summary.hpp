#pragma once

#include "yarda/cache/exact_csrd.hpp"
#include "yarda/cache/hierarchy_summary.hpp"

namespace yarda::detail
{

/**
 * @brief Validate and own a streaming cache level's semantic counts.
 * @param source Borrowed summary of a non-failed analyzer.
 * @return Owned counts and complete finite-distance histogram.
 * @throws std::logic_error if counts, histogram or historical lines disagree.
 * @throws std::overflow_error if conservation arithmetic overflows.
 */
CacheLevelSummary summarize_streaming_level(const ExactCsrdSummary & source);

/**
 * @brief Accumulate completed task coverage without wrapping counters.
 * @param total Borrowed module coverage to update; discard on failure.
 * @param task Borrowed completed source-to-L1 task coverage.
 * @return Nothing.
 * @throws std::overflow_error if any cumulative counter overflows.
 */
void add_streaming_coverage(TraceCoverage & total, const TraceCoverage & task);

/**
 * @brief Adapt one exact observation to the existing diagnostic event type.
 * @param observation Borrowed observation returned by a successful observe.
 * @return Owned decision with the same optional exact distance.
 * @throws std::overflow_error if the distance is not representable by size_t.
 */
LruAccessResult streaming_lru_result(const CsrdObservation & observation);

}  // namespace yarda::detail
