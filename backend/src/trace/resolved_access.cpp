#include "yarda/trace/resolved_access.hpp"

#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "block_trace.hpp"
#include "unroller.hpp"
#include "yarda/trace/calls.hpp"

namespace yarda
{
namespace
{

TraceCoverage coverage_delta(const TraceCoverage & after,
                             const TraceCoverage & before)
{
  return {
    after.source_accesses - before.source_accesses,
    after.resolved_accesses - before.resolved_accesses,
    after.rejected_accesses - before.rejected_accesses,
    after.emitted_line_references - before.emitted_line_references,
  };
}

}  // namespace

ResolvedTraceResult resolved_block_traces(const nlohmann::json & raw,
                                          const ObjectAddressModel & objects)
{
  try
  {
    const detail::AccessLayoutResolver layouts(raw);
    detail::ResolvedTraceUnroller unroller(objects, layouts);
    ResolvedTraceResult result;
    result.traces =
      detail::build_block_traces<NamedResolvedTrace, ResolvedAccess>(
        raw,
        [&unroller](const std::string & task_id, const nlohmann::json & node) {
          return unroller.unroll(node, task_id);
        },
        detail::EmptyLoopPolicy::Omit);
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
  try
  {
    const detail::AccessLayoutResolver layouts(raw);
    detail::ResolvedTraceUnroller unroller(objects, layouts);
    ResolvedTaskTraceResult result;
    const auto roots = expand_task_calls(raw);
    for (const auto & root : roots)
    {
      const auto task_id = root.at("function").get<std::string>();
      unroller.begin_task();
      const auto coverage_before = unroller.coverage();
      std::vector<ResolvedAccess> accesses;
      for (const auto & node : root.value("body", nlohmann::json::array()))
      {
        auto emitted = unroller.unroll(node, task_id);
        accesses.insert(accesses.end(),
                        std::make_move_iterator(emitted.begin()),
                        std::make_move_iterator(emitted.end()));
      }
      result.tasks.push_back(
        {task_id, std::move(accesses),
         coverage_delta(unroller.coverage(), coverage_before)});
    }
    result.coverage = unroller.coverage();
    return result;
  }
  catch (const nlohmann::json::exception & error)
  {
    throw std::invalid_argument("malformed LAT input: " +
                                std::string(error.what()));
  }
}

}  // namespace yarda
