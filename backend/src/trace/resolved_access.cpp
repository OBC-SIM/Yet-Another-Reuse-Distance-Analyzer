#include "yarda/trace/resolved_access.hpp"

#include <limits>
#include <stdexcept>
#include <string>

#include "block_trace.hpp"
#include "unroller.hpp"
#include "yarda/trace/task_access_stream.hpp"

namespace yarda
{

ResolvedTraceResult resolved_block_traces(const nlohmann::json & raw,
                                          const ObjectAddressModel & objects)
{
  try
  {
    const detail::AccessLayoutResolver layouts(raw);
    detail::ExpansionBudget expansion_budget;
    detail::ResolvedTraceUnroller unroller(objects, layouts, expansion_budget);
    ResolvedTraceResult result;
    result.traces =
      detail::build_block_traces<NamedResolvedTrace, ResolvedAccess>(
        raw,
        [&unroller](const std::string & task_id, const nlohmann::json & node) {
          return unroller.unroll(node, task_id);
        },
        detail::EmptyLoopPolicy::Omit, expansion_budget);
    result.coverage = unroller.coverage();
    return result;
  }
  catch (const nlohmann::json::exception & error)
  {
    throw std::invalid_argument("malformed LAT input: " +
                                std::string(error.what()));
  }
}

ResolvedTaskTraceResult resolved_task_traces(const nlohmann::json & raw,
                                             const ObjectAddressModel & objects)
{
  ResolvedTaskTraceResult result;
  const TaskAccessSink sink{
    [&](const std::string & task_id, std::uint64_t excluded) {
      result.tasks.push_back({task_id, {}, {}, excluded});
    },
    [&](const std::string &, const ResolvedAccess & access) {
      result.tasks.back().accesses.push_back(access);
    },
    [&](const std::string &, const TraceCoverage & coverage) {
      result.tasks.back().coverage = coverage;
    },
  };
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  TraceEmissionBudget budget({maximum, maximum});
  const auto summary =
    stream_resolved_task_accesses(raw, objects, sink, budget);
  result.coverage = summary.coverage;
  result.excluded_opaque_call_sites = summary.excluded_opaque_call_sites;
  return result;
}

}  // namespace yarda
