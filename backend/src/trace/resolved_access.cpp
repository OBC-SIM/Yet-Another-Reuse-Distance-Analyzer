#include "yarda/trace/resolved_access.hpp"

#include <stdexcept>
#include <string>

#include "block_trace.hpp"
#include "unroller.hpp"

namespace yarda
{

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

}  // namespace yarda
