#include "cache_line.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "yarda/cache/line_mapping.hpp"

namespace yarda::detail
{
namespace
{

std::int64_t floor_divide(std::int64_t dividend, std::int64_t divisor)
{
  auto quotient = dividend / divisor;
  if (dividend % divisor != 0 && dividend < 0)
  {
    --quotient;
  }
  return quotient;
}

}  // namespace

std::vector<std::string> trace_cache_line_keys(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size, const AccessLayoutResolver & layouts)
{
  if (line_size == 0 || line_size > static_cast<std::size_t>(
                                      std::numeric_limits<std::int64_t>::max()))
  {
    return {};
  }
  const auto access = layouts.resolve(node, indices);
  if (!access)
  {
    return {};
  }
  std::int64_t last_byte = 0;
  if (__builtin_add_overflow(access->offset, access->size - 1, &last_byte))
  {
    return {};
  }
  const auto divisor = static_cast<std::int64_t>(line_size);
  const auto first_line = floor_divide(access->offset, divisor);
  const auto last_line = floor_divide(last_byte, divisor);
  const auto key_base =
    has_field_path(node) ? node.value("object", "") : node.value("name", "");
  std::vector<std::string> keys;
  for (auto line = first_line;; ++line)
  {
    keys.push_back(key_base + "-line-" + std::to_string(line));
    if (line == last_line)
    {
      break;
    }
  }
  return keys;
}

CacheLineMapper::CacheLineMapper(const CacheGeometry & geometry,
                                 const ObjectAddressModel & objects,
                                 const AccessLayoutResolver & layouts)
  : geometry_(geometry), objects_(objects), layouts_(layouts)
{
  cache_set_count(geometry_);
}

std::vector<CacheLineMapping> CacheLineMapper::map(
  const nlohmann::json & node, const std::vector<std::string> & indices) const
{
  const auto object_id = node.value("object", "");
  if (object_id.rfind("global::", 0) != 0)
  {
    return {};
  }
  const auto access = layouts_.resolve(node, indices);
  if (!access)
  {
    if (!node.contains("elem_size"))
    {
      throw std::invalid_argument("global access lacks element size: " +
                                  object_id);
    }
    throw std::invalid_argument("global access offset is invalid: " +
                                object_id);
  }
  if (access->offset < 0)
  {
    throw std::invalid_argument("global access offset is negative: " +
                                object_id);
  }
  return map_cache_lines(object_id, static_cast<std::uint64_t>(access->offset),
                         static_cast<std::uint64_t>(access->size), objects_,
                         geometry_);
}

}  // namespace yarda::detail
