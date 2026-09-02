#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "access_layout.hpp"

namespace yarda::detail
{

std::vector<std::string> trace_cache_line_keys(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size, const AccessLayoutResolver & layouts);

}  // namespace yarda::detail
