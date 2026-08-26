#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

#include "access_layout.hpp"

namespace yarda::detail
{

std::optional<ByteAccess> resolve_legacy_access(
  const nlohmann::json & node, const std::vector<std::string> & indices);

}  // namespace yarda::detail
