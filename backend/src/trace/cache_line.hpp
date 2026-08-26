#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "access_layout.hpp"
#include "yarda/cache/line_mapping.hpp"

namespace yarda::detail
{

std::vector<std::string> trace_cache_line_keys(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size, const AccessLayoutResolver & layouts);

class CacheLineMapper
{
public:
  CacheLineMapper(const CacheGeometry & geometry,
                  const ObjectAddressModel & objects,
                  const AccessLayoutResolver & layouts);

  std::vector<CacheLineMapping>
  map(const nlohmann::json & node,
      const std::vector<std::string> & indices) const;

private:
  const CacheGeometry & geometry_;
  const ObjectAddressModel & objects_;
  const AccessLayoutResolver & layouts_;
};

}  // namespace yarda::detail
