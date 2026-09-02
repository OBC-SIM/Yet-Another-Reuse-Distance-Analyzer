#include "yarda/trace/resolved_access.hpp"

#include "block_trace.hpp"
#include "unroller.hpp"

namespace yarda
{

ResolvedTraceResult resolved_block_traces(const nlohmann::json & raw,
                                          const ObjectAddressModel & objects)
{
  const detail::AccessLayoutResolver layouts(raw);
  detail::ResolvedTraceUnroller unroller(objects, layouts);
  ResolvedTraceResult result;
  result.traces =
    detail::build_block_traces<NamedResolvedTrace, ResolvedAccess>(
      raw,
      [&unroller](const nlohmann::json & node) {
        return unroller.unroll(node);
      },
      detail::EmptyLoopPolicy::Omit);
  return result;
}

}  // namespace yarda
