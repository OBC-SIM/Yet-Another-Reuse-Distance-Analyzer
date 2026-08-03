#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "yarda/cache_line_mapping.hpp"

namespace yarda::detail
{

std::optional<std::string> trace_cache_line_key(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size, const CacheGeometry * geometry,
  const ObjectAddressModel * objects, CacheLineMappingTable * mappings);

}  // namespace yarda::detail
