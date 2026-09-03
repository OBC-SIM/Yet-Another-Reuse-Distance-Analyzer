#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>

namespace yarda::detail
{

using CallMapping = std::unordered_map<std::string, std::string>;

nlohmann::json substitute_call_node(nlohmann::json node,
                                    const CallMapping & names,
                                    const CallMapping & object_ids,
                                    const nlohmann::json & objects);

}  // namespace yarda::detail
