#include "access_layout_legacy.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "prepared_layout.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

std::optional<std::vector<std::int64_t>>
parse_indices(const std::vector<std::string> & indices)
{
  std::vector<std::int64_t> numeric;
  numeric.reserve(indices.size());
  for (const auto & index : indices)
  {
    const auto parsed = parse_exact_integer(index);
    if (!parsed)
    {
      return std::nullopt;
    }
    numeric.push_back(*parsed);
  }
  return numeric;
}

std::optional<std::int64_t>
linear_index(const Json & node, const std::vector<std::int64_t> & indices,
             PreparedLayout * plan)
{
  if (indices.empty())
  {
    return node.value("type", "") == "Scalar" ? std::optional<std::int64_t>(0)
                                              : std::nullopt;
  }
  if (indices.size() == 1)
  {
    if (plan) plan->steps.push_back({0, 1});
    return indices.front();
  }
  if (!node.contains("shape") || !node["shape"].is_array() ||
      (node["shape"].size() != indices.size() &&
       node["shape"].size() + 1 != indices.size()))
  {
    return std::nullopt;
  }

  const auto required = indices.size() - 1;
  const auto start = node["shape"].size() - required;
  std::vector<std::int64_t> shape;
  for (std::size_t index = start; index < node["shape"].size(); ++index)
  {
    if (!node["shape"][index].is_number_integer())
    {
      return std::nullopt;
    }
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
    if (plan) plan->steps.push_back({position, stride});
  }
  return linear;
}

}  // namespace

std::optional<ByteAccess> resolve_legacy_access(
  const nlohmann::json & node, const std::vector<std::string> & indices)
{
  if (!node.contains("elem_size") || !node["elem_size"].is_number_integer())
  {
    return std::nullopt;
  }
  const auto numeric = parse_indices(indices);
  if (!numeric)
  {
    return std::nullopt;
  }
  return prepare_legacy_access(node, *numeric, nullptr);
}

std::optional<ByteAccess> prepare_legacy_access(
  const nlohmann::json & node, const std::vector<std::int64_t> & indices,
  PreparedLayout * plan)
{
  if (!node.contains("elem_size") || !node["elem_size"].is_number_integer())
    return std::nullopt;
  const auto linear = linear_index(node, indices, plan);
  const auto size = node["elem_size"].get<std::int64_t>();
  std::int64_t offset = 0;
  if (!linear || size <= 0 || __builtin_mul_overflow(*linear, size, &offset))
  {
    return std::nullopt;
  }
  if (plan) plan->width = size;
  return ByteAccess{offset, size};
}

}  // namespace yarda::detail
