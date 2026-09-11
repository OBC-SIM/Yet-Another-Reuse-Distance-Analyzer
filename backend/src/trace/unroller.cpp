#include "unroller.hpp"

#include <sstream>
#include <string>
#include <vector>

#include "access_resolver.hpp"
#include "prepared_trace.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

}  // namespace

TraceUnroller::TraceUnroller(Granularity granularity,
                             std::size_t cache_line_size,
                             const AccessLayoutResolver & layouts,
                             ExpansionBudget & expansion_budget)
  : granularity_(granularity)
  , cache_line_size_(cache_line_size)
  , layouts_(layouts)
  , expansion_budget_(expansion_budget)
{
}

std::vector<std::string>
TraceUnroller::unroll(const nlohmann::json & node) const
{
  std::vector<std::string> trace;
  const auto emit = [&](const Json & access,
                        const std::vector<std::string> & indices) {
    if (access.value("type", "") == "Scalar")
    {
      trace.push_back(access.value("name", ""));
      return;
    }
    if (granularity_ == Granularity::CacheLine)
    {
      const auto cache_lines =
        trace_cache_line_keys(access, indices, cache_line_size_, layouts_);
      if (!cache_lines.empty())
      {
        trace.insert(trace.end(), cache_lines.begin(), cache_lines.end());
        return;
      }
    }
    std::ostringstream key;
    key << access.value("name", "");
    for (const auto & index : indices)
    {
      key << '-' << index;
    }
    trace.push_back(key.str());
  };
  visit_prepared_trace(node, expansion_budget_, emit);
  return trace;
}

ResolvedTraceUnroller::ResolvedTraceUnroller(
  const ObjectAddressModel & objects, const AccessLayoutResolver & layouts,
  ExpansionBudget & expansion_budget, TraceEmissionBudget * emission_budget)
  : objects_(objects)
  , layouts_(layouts)
  , expansion_budget_(expansion_budget)
  , emission_budget_(emission_budget)
{
}

std::vector<ResolvedAccess> ResolvedTraceUnroller::unroll(
  const nlohmann::json & node, const std::string & task_id)
{
  std::vector<ResolvedAccess> accesses;
  unroll(node, task_id,
         [&](const ResolvedAccess & access) { accesses.push_back(access); });
  return accesses;
}

void ResolvedTraceUnroller::unroll(const nlohmann::json & node,
                                   const std::string & task_id,
                                   const ResolvedAccessSink & sink)
{
  const auto emit = [&](const Json & access, PreparedAccess & prepared,
                        const std::vector<std::int64_t> & slots) {
    const auto exact = prepared.evaluate_numeric(slots);
    if (emission_budget_) emission_budget_->consume_source_access();
    const auto ordinal = next_source_access_ordinal_++;
    ++coverage_.source_accesses;
    const auto resolved = resolve_access(
      access, prepared, exact, task_id, ordinal, objects_, layouts_, coverage_);
    sink(resolved);
  };
  visit_prepared_accesses(node, expansion_budget_, emit);
}

void ResolvedTraceUnroller::begin_task() noexcept
{
  next_source_access_ordinal_ = 0;
}

const TraceCoverage & ResolvedTraceUnroller::coverage() const noexcept
{
  return coverage_;
}

}  // namespace yarda::detail
