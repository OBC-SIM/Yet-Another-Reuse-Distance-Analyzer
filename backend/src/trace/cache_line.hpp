#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "yarda/cache/line_mapping.hpp"

namespace yarda::detail
{

struct TraceCacheLine
{
  std::string key;
  std::optional<CacheLineMapping> mapping;
};

std::optional<TraceCacheLine> trace_cache_line(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size, const CacheGeometry * geometry,
  const ObjectAddressModel * objects, CacheLineMappingTable * mappings);

}  // namespace yarda::detail
