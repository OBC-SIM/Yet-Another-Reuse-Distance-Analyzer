#pragma once

#include <nlohmann/json.hpp>

#include <exception>
#include <utility>

#include "yarda/trace/trace_coverage.hpp"

namespace yarda::detail
{

/** @brief Separate consumer failures from malformed LAT JSON exceptions. */
struct TaskSinkFailure
{
  std::exception_ptr exception;
};

template <typename Callback, typename... Arguments>
void invoke_task_sink(const Callback & callback, Arguments &&... arguments)
{
  try
  {
    callback(std::forward<Arguments>(arguments)...);
  }
  catch (...)
  {
    throw TaskSinkFailure{std::current_exception()};
  }
}

TraceCoverage coverage_delta(const TraceCoverage & after,
                             const TraceCoverage & before);

nlohmann::json remove_opaque_calls(nlohmann::json body,
                                   std::uint64_t & excluded);

}  // namespace yarda::detail
