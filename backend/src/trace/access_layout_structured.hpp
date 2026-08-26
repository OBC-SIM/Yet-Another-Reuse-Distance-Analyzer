#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "access_layout.hpp"

namespace yarda::detail
{

ByteAccess resolve_structured_access(const nlohmann::json & node,
                                     const std::vector<std::string> & indices,
                                     const nlohmann::json & objects,
                                     const nlohmann::json & structures);

}  // namespace yarda::detail
