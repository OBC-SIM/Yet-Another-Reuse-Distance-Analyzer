#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cache_line.hpp"
#include "expansion_budget.hpp"
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

class ResolvedTraceUnroller
{
public:
  ResolvedTraceUnroller(const ObjectAddressModel & objects,
                        const AccessLayoutResolver & layouts,
                        ExpansionBudget & expansion_budget);

  std::vector<ResolvedAccess> unroll(const nlohmann::json & node,
                                     const std::string & task_id);

  /** Reset source ordinals without discarding aggregate resolution coverage. */
  void begin_task() noexcept;

  const TraceCoverage & coverage() const noexcept;

private:
  const ObjectAddressModel & objects_;
  const AccessLayoutResolver & layouts_;
  ExpansionBudget & expansion_budget_;
  TraceCoverage coverage_;
  std::uint64_t next_source_access_ordinal_ = 0;
};

}  // namespace yarda::detail
