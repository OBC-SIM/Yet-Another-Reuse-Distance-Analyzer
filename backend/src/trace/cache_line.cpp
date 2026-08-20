#include "cache_line.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "yarda/cache/line_mapping.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

struct ByteAccess
{
  std::int64_t offset;
  std::int64_t size;
};

std::optional<std::int64_t> parse_integer(const std::string & value)
{
  try
  {
    std::size_t consumed = 0;
    const auto parsed = std::stoll(value, &consumed);
    return consumed == value.size() ? std::optional<std::int64_t>(parsed)
                                    : std::nullopt;
  }
  catch (const std::exception &)
  {
    return std::nullopt;
  }
}

std::int64_t floor_divide(std::int64_t dividend, std::int64_t divisor)
{
  auto quotient = dividend / divisor;
  if (dividend % divisor != 0 && dividend < 0)
  {
    --quotient;
  }
  return quotient;
}

bool has_field_path(const Json & node)
{
  for (const auto & segment : node.value("access_path", Json::array()))
  {
    if (segment.value("kind", "") == "field")
    {
      return true;
    }
  }
  return false;
}

std::optional<std::int64_t>
linear_index(const Json & node, const std::vector<std::int64_t> & indices)
{
  if (indices.size() == 1)
  {
    return indices.front();
  }
  if (!node.contains("shape") || !node["shape"].is_array() ||
      (node["shape"].size() != indices.size() &&
       node["shape"].size() + 1 != indices.size()))
  {
    return std::nullopt;
  }

  std::vector<std::int64_t> shape;
  const auto required = indices.size() - 1;
  const auto start = node["shape"].size() - required;
  for (std::size_t index = start; index < node["shape"].size(); ++index)
  {
    shape.push_back(node["shape"][index].get<std::int64_t>());
  }
  std::int64_t linear = 0;
  for (std::size_t position = 0; position < indices.size(); ++position)
  {
    std::int64_t stride = 1;
    for (std::size_t dimension = position; dimension < shape.size();
         ++dimension)
    {
      if (__builtin_mul_overflow(stride, shape[dimension], &stride))
      {
        return std::nullopt;
      }
    }
    std::int64_t term = 0;
    if (__builtin_mul_overflow(indices[position], stride, &term) ||
        __builtin_add_overflow(linear, term, &linear))
    {
      return std::nullopt;
    }
  }
  return linear;
}

std::optional<std::vector<std::int64_t>>
parse_indices(const std::vector<std::string> & indices)
{
  std::vector<std::int64_t> numeric;
  for (const auto & index : indices)
  {
    const auto parsed = parse_integer(index);
    if (!parsed)
    {
      return std::nullopt;
    }
    numeric.push_back(*parsed);
  }
  return numeric;
}

std::optional<ByteAccess>
resolve_byte_access(const Json & node,
                    const std::vector<std::int64_t> & indices)
{
  const auto linear = linear_index(node, indices);
  const auto element_size = node["elem_size"].get<std::int64_t>();
  std::int64_t byte_offset = 0;
  if (!linear || element_size <= 0 ||
      __builtin_mul_overflow(*linear, element_size, &byte_offset))
  {
    return std::nullopt;
  }
  return ByteAccess{byte_offset, element_size};
}

}  // namespace

std::optional<std::string> trace_cache_line_key(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size)
{
  if (!node.contains("elem_size") || line_size == 0)
  {
    return std::nullopt;
  }
  const auto numeric = parse_indices(indices);
  if (!numeric)
  {
    return std::nullopt;
  }
  const auto access = resolve_byte_access(node, *numeric);
  if (!access)
  {
    return std::nullopt;
  }
  return node.value("name", "") + "-line-" +
         std::to_string(floor_divide(
           access->offset, static_cast<std::int64_t>(line_size)));
}

CacheLineMapper::CacheLineMapper(const CacheGeometry & geometry,
                                 const ObjectAddressModel & objects)
  : geometry_(geometry), objects_(objects)
{
  cache_set_count(geometry_);
}

std::optional<CacheLineMapping>
CacheLineMapper::map(const nlohmann::json & node,
                     const std::vector<std::string> & indices) const
{
  const auto object_id = node.value("object", "");
  if (object_id.rfind("global::", 0) != 0)
  {
    return std::nullopt;
  }
  if (!node.contains("elem_size"))
  {
    throw std::invalid_argument("global access lacks element size: " +
                                object_id);
  }
  if (has_field_path(node))
  {
    throw std::invalid_argument(
      "structured global access mapping is not supported: " + object_id);
  }
  const auto numeric = parse_indices(indices);
  if (!numeric)
  {
    throw std::invalid_argument("global access index is not exact: " +
                                object_id);
  }
  const auto access = resolve_byte_access(node, *numeric);
  if (!access)
  {
    throw std::invalid_argument("global access offset is invalid: " +
                                object_id);
  }
  if (access->offset < 0)
  {
    throw std::invalid_argument("global access offset is negative: " +
                                object_id);
  }
  return map_cache_line(object_id, static_cast<std::uint64_t>(access->offset),
                        static_cast<std::uint64_t>(access->size), objects_,
                        geometry_);
}

}  // namespace yarda::detail
