#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "yarda/cache/line_mapping.hpp"

namespace yarda::detail
{

std::optional<std::string> trace_cache_line_key(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size);

class CacheLineMapper
{
public:
  CacheLineMapper(const CacheGeometry & geometry,
                  const ObjectAddressModel & objects);

  std::optional<CacheLineMapping>
  map(const nlohmann::json & node,
      const std::vector<std::string> & indices) const;

private:
  const CacheGeometry & geometry_;
  const ObjectAddressModel & objects_;
};

}  // namespace yarda::detail
