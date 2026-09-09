#pragma once

#include "yarda/cache/exact_csrd.hpp"
#include "yarda/cache/streaming_hierarchy.hpp"

namespace yarda::detail
{

/**
 * @brief Own one task's cold histories and counters without reference vectors.
 *
 * Destroy on any failure. finish is called once, after all sources succeeded.
 * Nonmovable analyzers and a callback bound to this instance require stable
 * ownership. Borrowed hierarchy, budget, options and delivery outlive the task.
 */
class StreamingHierarchyTask
{
public:
  /**
   * @brief Create independent L1/LLC histories for a validated task identity.
   * @param id Owned nonempty task identity, unique in the module.
   * @param hierarchy Borrowed selected equal-line-size hierarchy.
   * @param budget Borrowed module budget shared with source production.
   * @param options Borrowed validated event options, unchanged during analysis.
   * @param delivery Borrowed module-wide diagnostic delivery counters.
   */
  StreamingHierarchyTask(std::string id, const AnalysisHierarchy & hierarchy,
                         TraceEmissionBudget & budget,
                         const StreamingHierarchyOptions & options,
                         HierarchyEventDelivery & delivery);

  /**
   * @brief Map one source and synchronously propagate each L1 miss to LLC.
   * @param access Borrowed absolute load/store with the next source ordinal.
   * @return Nothing.
   * @throws std::invalid_argument for unsupported sources or emission limits.
   * @throws std::overflow_error for mapping or counter overflow.
   * @note Sink and analyzer exceptions propagate; destroy this task on failure.
   */
  void accept(const ResolvedAccess & access);

  /**
   * @brief Validate and transfer the completed task's semantic summary.
   * @param coverage Borrowed source-only coverage from the task end callback.
   * @return Owned summary with complete source-to-L1 coverage and invariants.
   * @throws std::logic_error for inconsistent source/level/service counts.
   * @throws std::overflow_error for conservation arithmetic overflow.
   */
  TaskHierarchySummary finish(const TraceCoverage & coverage);

private:
  void observe_line(const CacheLineMapping & mapping);

  const AnalysisHierarchy & hierarchy_;
  TraceEmissionBudget & budget_;
  const StreamingHierarchyOptions & options_;
  HierarchyEventDelivery & delivery_;
  ExactCsrdAnalyzer l1_;
  ExactCsrdAnalyzer llc_;
  TaskHierarchySummary summary_;
  std::uint64_t next_span_ordinal_ = 0;
  CacheLineSink line_sink_;
};

}  // namespace yarda::detail
