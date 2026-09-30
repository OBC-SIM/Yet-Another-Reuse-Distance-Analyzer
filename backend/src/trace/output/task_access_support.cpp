#include "task_access_support.hpp"

namespace yarda::detail
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

nlohmann::json remove_opaque_calls(nlohmann::json body,
                                   std::uint64_t & excluded)
{
  auto result = nlohmann::json::array();
  for (auto & node : body)
  {
    const auto type = node.value("type", "");
    if (type == "Call")
    {
      ++excluded;
      continue;
    }
    if (type == "Loop")
    {
      node["body"] = remove_opaque_calls(std::move(node["body"]), excluded);
    }
    result.push_back(std::move(node));
  }
  return result;
}

}  // namespace yarda::detail
