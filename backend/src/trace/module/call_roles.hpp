#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace yarda::detail
{
namespace call_roles
{

inline bool has_role(const nlohmann::json & function, const char * ape_role,
                     const char * yard_role)
{
  for (const auto & annotation :
       function.value("annotations", nlohmann::json::array()))
  {
    const auto value = annotation.get<std::string>();
    if (value == ape_role || value == yard_role)
    {
      return true;
    }
  }
  return false;
}

inline bool is_analyzed(const nlohmann::json & function)
{
  return has_role(function, "ape.analyze", "yard.analyze");
}

inline bool is_inline(const nlohmann::json & function)
{
  return has_role(function, "ape.inline", "yard.inline");
}

}  // namespace call_roles
}  // namespace yarda::detail
