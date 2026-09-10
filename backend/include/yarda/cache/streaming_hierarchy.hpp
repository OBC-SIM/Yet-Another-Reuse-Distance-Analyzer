#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "yarda/cache/hierarchy_analysis.hpp"
#include "yarda/trace/emission_budget.hpp"
#include "yarda/trace/work_limits.hpp"

namespace yarda
{

/**
 * @brief Receive one diagnostic event synchronously with its task identity.
 *
 * Arguments are borrowed only during the callback. Copy retained events. Any
 * exception aborts analysis unchanged; discard all collected events on failure,
 * including events from earlier tasks. A sink must not mutate analysis inputs.
 */
using HierarchyEventSink =
  std::function<void(const std::string &, const HierarchyAccessEvent &)>;

/** @brief Configure one complete streaming module execution. */
struct StreamingHierarchyOptions
{
  /**
   * @brief Shared source/line limits, cumulative across every task.
   *
   * Defaults to 1,000,000 sources and 10,000,000 source-to-L1 line references.
   * Batch adapters do not impose these emission limits; structural expansion
   * limits still apply to both paths. Exhaustion fails the entire analysis.
   */
  TraceEmissionLimits emission_limits;
  /** @brief Optional diagnostics consumer; empty selects summary-only mode. */
  HierarchyEventSink event_sink;
  /**
   * @brief Maximum event deliveries across the module, inclusive.
   *
   * Zero with a sink delivers no events; analysis still processes every line.
   * A nonzero limit requires a sink. Truncation never consumes emission budget.
   */
  std::uint64_t event_limit = 0;
  /**
   * @brief Single-loop and module-cumulative allowances; zero forbids
   * iterations.
   *
   * Both default to 1,000,000. Exhaustion fails the entire invocation without
   * a partial result. Legacy batch adapters retain the default loop limits.
   */
  LoopWorkLimits loop_limits{};
};

/** @brief Diagnostic delivery metadata, separate from semantic task counts. */
struct HierarchyEventDelivery
{
  /** @brief Number of callbacks that returned successfully across all tasks. */
  std::uint64_t emitted_events = 0;
  /** @brief True only when an enabled sink omits a reference past its limit. */
  bool events_truncated = false;
};

/** @brief Own complete cold-task summaries without per-reference vectors. */
struct StreamingHierarchyResult
{
  /** @brief Owned summaries in source task order, including empty tasks. */
  std::vector<TaskHierarchySummary> tasks;
  /** @brief Aggregate source resolution and actual source-to-L1 emissions. */
  TraceCoverage coverage;
  /** @brief Diagnostics metadata; event options do not affect semantic counts.
   */
  HierarchyEventDelivery event_delivery;
};

/**
 * @brief Stream exact L1/LLC analysis of independent, cold-started LAT tasks.
 *
 * Preserves batch semantics, task order and empty tasks. Only L1 misses touch
 * LLC, decoded from the original linked address with LLC geometry. One module
 * budget charges sources and source-to-L1 lines, never LLC forwarding twice.
 * Summary mode retains no resolved/mapped trace or event vector. Input DOM,
 * expanded calls, completed summaries and full distinct-line history still
 * occupy memory. Event limits affect diagnostics only.
 *
 * @param raw Borrowed APE v2 LAT, unchanged throughout analysis.
 * @param objects Borrowed absolute linked object model, unchanged throughout.
 * @param hierarchy Borrowed selected model snapshot, unchanged throughout.
 * @param options Borrowed limits and optional sink, unchanged throughout.
 * @pre hierarchy is an unmodified result of select_analysis_hierarchy().
 * @return Owned summaries and source-to-L1 coverage only on complete success.
 * @throws std::invalid_argument for unsupported input, opaque calls, invalid
 * geometry/options or exhausted expansion/emission limits.
 * @throws ResolutionError for unsupported or unresolved source accesses.
 * @throws std::overflow_error for address, distance, storage or count overflow.
 * @throws std::logic_error for inconsistent coverage or hierarchy invariants.
 * @note Any exception invalidates the whole invocation. No partial result is
 * returned; callers must discard all sink state, even for completed tasks.
 */
StreamingHierarchyResult analyze_streaming_hierarchy(
  const nlohmann::json & raw, const ObjectAddressModel & objects,
  const AnalysisHierarchy & hierarchy,
  const StreamingHierarchyOptions & options = {});

}  // namespace yarda
