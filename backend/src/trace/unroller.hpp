#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "cache_line.hpp"
#include "expansion_budget.hpp"
#include "yarda/trace/emission_budget.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/trace.hpp"
#include "yarda/trace/trace_coverage.hpp"

namespace yarda::detail
{

class TraceUnroller
{
public:
  TraceUnroller(Granularity granularity, std::size_t cache_line_size,
                const AccessLayoutResolver & layouts,
                ExpansionBudget & expansion_budget);

  std::vector<std::string> unroll(const nlohmann::json & node) const;

private:
  Granularity granularity_;
  std::size_t cache_line_size_;
  const AccessLayoutResolver & layouts_;
  ExpansionBudget & expansion_budget_;
};

/** @brief Synchronous source consumer; access references expire on return. */
using ResolvedAccessSink = std::function<void(const ResolvedAccess &)>;

class ResolvedTraceUnroller
{
public:
  /**
   * @brief Borrow resolution inputs and optional source-emission accounting.
   * @param objects Borrowed object addresses, alive throughout traversal.
   * @param layouts Borrowed layouts, alive throughout traversal.
   * @param expansion_budget Borrowed cumulative structural-work budget.
   * @param emission_budget Borrowed nullable emission budget; null retains
   * legacy block limits. Ownership stays with the caller.
   */
  ResolvedTraceUnroller(const ObjectAddressModel & objects,
                        const AccessLayoutResolver & layouts,
                        ExpansionBudget & expansion_budget,
                        TraceEmissionBudget * emission_budget = nullptr);

  std::vector<ResolvedAccess> unroll(const nlohmann::json & node,
                                     const std::string & task_id);

  /**
   * @brief Resolve and deliver accesses without retaining a trace vector.
   * @param node Borrowed LAT subtree, unchanged throughout traversal. Prepared
   * nodes and loop slots are owned only until this traversal returns or fails.
   * @param task_id Borrowed identity for source-resolution diagnostics.
   * @param sink Required synchronous consumer, configured before traversal.
   * @return Nothing.
   * @note Any exception stops traversal; discard this unroller on failure.
   */
  void unroll(const nlohmann::json & node, const std::string & task_id,
              const ResolvedAccessSink & sink);

  /** Reset source ordinals without discarding aggregate resolution coverage. */
  void begin_task() noexcept;

  const TraceCoverage & coverage() const noexcept;

private:
  const ObjectAddressModel & objects_;
  const AccessLayoutResolver & layouts_;
  ExpansionBudget & expansion_budget_;
  TraceEmissionBudget * emission_budget_;
  TraceCoverage coverage_;
  std::uint64_t next_source_access_ordinal_ = 0;
};

}  // namespace yarda::detail
