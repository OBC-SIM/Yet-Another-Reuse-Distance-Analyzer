#include "yarda/trace/trace.hpp"

#include <string>
#include <vector>

#include "block_trace.hpp"
#include "unroller.hpp"

namespace yarda
{

std::vector<std::string> unroll_node_actual(const nlohmann::json & node,
                                            Granularity granularity,
                                            std::size_t cache_line_size)
{
  const detail::AccessLayoutResolver layouts;
  return detail::TraceUnroller(granularity, cache_line_size, layouts)
    .unroll(node);
}

std::vector<NamedTrace> block_traces(const nlohmann::json & raw,
                                     Granularity granularity,
                                     std::size_t cache_line_size)
{
  const detail::AccessLayoutResolver layouts(raw);
  const detail::TraceUnroller unroller(granularity, cache_line_size, layouts);
  return detail::build_block_traces<NamedTrace, std::string>(
    raw,
    [&unroller](const std::string &, const nlohmann::json & node) {
      return unroller.unroll(node);
    },
    detail::EmptyLoopPolicy::Include);
}

}  // namespace yarda
