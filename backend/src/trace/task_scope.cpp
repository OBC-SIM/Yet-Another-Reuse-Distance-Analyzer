#include "task_scope.hpp"

#include <stdexcept>
#include <unordered_set>

#include "call_roles.hpp"

namespace yarda::detail
{

std::string analysis_task_id(const nlohmann::json & function)
{
  const auto name = function.at("function").get<std::string>();
  if (!function.contains("analysis_scope")) return name;
  return "region:" + std::to_string(name.size()) + ":" + name + ":APE_ANALYZE";
}

void validate_task_scopes(const nlohmann::json & functions, bool task_mode)
{
  std::unordered_set<std::string> identities;
  for (const auto & function : functions)
  {
    if (function.contains("analysis_scope"))
    {
      if (!task_mode)
        throw std::invalid_argument(
          "legacy module expansion rejects analysis_scope");
      const auto & scope = function.at("analysis_scope");
      if (!scope.is_object() || scope.size() != 2 || !scope.contains("kind") ||
          scope.at("kind") != "region" || !scope.contains("name") ||
          scope.at("name") != "APE_ANALYZE")
        throw std::invalid_argument(
          "invalid analysis_scope: expected APE_ANALYZE region");
      if (!call_roles::is_analyzed(function) || call_roles::is_inline(function))
        throw std::invalid_argument(
          "analysis_scope requires a non-inline analyzed root");
    }
    if (task_mode && call_roles::is_analyzed(function))
    {
      const auto id = analysis_task_id(function);
      if (!identities.insert(id).second)
        throw std::invalid_argument("Duplicate task identity: " + id);
    }
  }
}

}  // namespace yarda::detail
