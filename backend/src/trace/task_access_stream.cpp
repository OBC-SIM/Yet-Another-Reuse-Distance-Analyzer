#include "yarda/trace/task_access_stream.hpp"

#include <stdexcept>

#include "call_expansion.hpp"
#include "task_access_support.hpp"
#include "unroller.hpp"

namespace yarda
{

TaskAccessStreamResult stream_resolved_task_accesses(
  const nlohmann::json & raw, const ObjectAddressModel & objects,
  const TaskAccessSink & sink, TraceEmissionBudget & budget)
{
  if (!sink.begin_task || !sink.access || !sink.end_task)
  {
    throw std::invalid_argument("task access sink requires all callbacks");
  }
  try
  {
    const detail::AccessLayoutResolver layouts(raw);
    detail::ExpansionBudget expansion_budget;
    detail::ResolvedTraceUnroller unroller(objects, layouts, expansion_budget,
                                           &budget);
    TaskAccessStreamResult result;
    auto roots = detail::expand_task_calls(raw, expansion_budget);
    for (auto & root : roots)
    {
      const auto task_id = root.at("function").get<std::string>();
      std::uint64_t excluded = 0;
      const auto body =
        detail::remove_opaque_calls(std::move(root["body"]), excluded);
      unroller.begin_task();
      const auto before = unroller.coverage();
      detail::invoke_task_sink(sink.begin_task, task_id, excluded);
      const detail::ResolvedAccessSink access_sink =
        [&](const ResolvedAccess & access) {
          detail::invoke_task_sink(sink.access, task_id, access);
        };
      for (const auto & node : body)
      {
        unroller.unroll(node, task_id, access_sink);
      }
      detail::invoke_task_sink(
        sink.end_task, task_id,
        detail::coverage_delta(unroller.coverage(), before));
      result.excluded_opaque_call_sites += excluded;
    }
    result.coverage = unroller.coverage();
    return result;
  }
  catch (const detail::TaskSinkFailure & failure)
  {
    std::rethrow_exception(failure.exception);
  }
  catch (const nlohmann::json::exception & error)
  {
    throw std::invalid_argument("malformed LAT input: " +
                                std::string(error.what()));
  }
}

TaskAccessStreamResult stream_resolved_task_accesses(
  const nlohmann::json & raw, const ObjectAddressModel & objects,
  const TaskAccessSink & sink)
{
  TraceEmissionBudget budget;
  return stream_resolved_task_accesses(raw, objects, sink, budget);
}

}  // namespace yarda
