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

using Json = nlohmann::json;

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

Json remove_opaque_calls(const Json & body, std::uint64_t & excluded)
{
  Json result = Json::array();
  for (auto node : body)
  {
    const auto type = node.value("type", "");
    if (type == "Call")
    {
      ++excluded;
      continue;
    }
    if (type == "Loop")
    {
      node["body"] =
        remove_opaque_calls(node.value("body", Json::array()), excluded);
    }
    result.push_back(std::move(node));
  }
  return result;
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
      std::uint64_t excluded_opaque_call_sites = 0;
      const auto body =
        remove_opaque_calls(root.value("body", nlohmann::json::array()),
                            excluded_opaque_call_sites);
      unroller.begin_task();
      const auto coverage_before = unroller.coverage();
      std::vector<ResolvedAccess> accesses;
      for (const auto & node : body)
      {
        auto emitted = unroller.unroll(node, task_id);
        accesses.insert(accesses.end(),
                        std::make_move_iterator(emitted.begin()),
                        std::make_move_iterator(emitted.end()));
      }
      result.tasks.push_back(
        {task_id, std::move(accesses),
         coverage_delta(unroller.coverage(), coverage_before),
         excluded_opaque_call_sites});
      result.excluded_opaque_call_sites += excluded_opaque_call_sites;
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
