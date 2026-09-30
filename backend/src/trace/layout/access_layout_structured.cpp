#include "access_layout_structured.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "access_layout_structured_type.hpp"
#include "prepared_layout.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

using namespace structured;

void add_offset(std::int64_t amount, std::int64_t & offset,
                const std::string & object_id)
{
  if (amount < 0 || __builtin_add_overflow(offset, amount, &offset))
  {
    reject(object_id, "byte offset overflows");
  }
}

void select_field(const Json & structures, const Json & segment,
                  const std::string & object_id, TypeState & state,
                  std::int64_t & offset, PreparedLayout * plan)
{
  const auto & field = find_field(structures, state, segment, object_id);
  const auto field_offset = required_integer(field, "offset", object_id);
  const auto field_size = required_integer(field, "size", object_id);
  if (field_offset < 0 || field_size <= 0 || field_size > state.element_size ||
      field_offset > state.element_size - field_size)
  {
    reject(object_id, "field exceeds its containing structure");
  }
  add_offset(field_offset, offset, object_id);
  state = type_state(field, object_id);
  if (type_extent(state, object_id) != field_size)
  {
    reject(object_id, "field size disagrees with its type");
  }
  if (plan) plan->steps.push_back({std::nullopt, field_offset});
}

void select_index(std::int64_t index, const std::string & object_id,
                  TypeState & state, std::int64_t & offset,
                  std::size_t position, PreparedLayout * plan)
{
  const auto dimension = state.shape.empty() ? 0 : state.shape.front();
  std::int64_t stride = state.element_size;
  if (!state.shape.empty())
  {
    if (index >= state.shape.front())
    {
      reject(object_id, "index exceeds its dimension");
    }
    for (std::size_t dimension = 1; dimension < state.shape.size(); ++dimension)
    {
      if (__builtin_mul_overflow(stride, state.shape[dimension], &stride))
      {
        reject(object_id, "index stride overflows");
      }
    }
    state.shape.erase(state.shape.begin());
  }
  else
  {
    reject(object_id, "index does not select an array or pointer");
  }
  std::int64_t indexed_offset = 0;
  if (__builtin_mul_overflow(index, stride, &indexed_offset))
  {
    reject(object_id, "indexed byte offset overflows");
  }
  add_offset(indexed_offset, offset, object_id);
  if (plan) plan->steps.push_back({position, stride, dimension});
}

std::optional<std::int64_t> numeric_index(const std::string & value)
{
  return parse_exact_integer(value);
}

std::optional<std::int64_t> numeric_index(std::int64_t value) { return value; }

template <typename Index>
ByteAccess resolve_path(const nlohmann::json & node,
                        const std::vector<Index> & indices,
                        const nlohmann::json & objects,
                        const nlohmann::json & structures,
                        PreparedLayout * plan)
{
  const auto object_id = node.value("object", "");
  if (object_id.empty() || !objects.is_object() || !objects.contains(object_id))
  {
    const auto label = object_id.empty() ? "<unknown>" : object_id;
    reject(label, "lacks object metadata");
  }
  if (!node.contains("indices") || !node["indices"].is_array() ||
      node["indices"].size() != indices.size())
  {
    reject(object_id, "path and node indices disagree");
  }
  const auto & object = objects.at(object_id);
  auto state = type_state(object, object_id);
  if (state.kind == "pointer")
  {
    reject(object_id, "uses unsupported pointer object metadata");
  }
  const auto object_extent = type_extent(state, object_id);
  if (plan)
  {
    plan->structured = true;
    plan->extent = object_extent;
  }
  std::int64_t offset = 0;
  std::size_t index_position = 0;

  const auto & path = node["access_path"];
  for (std::size_t path_position = 0; path_position < path.size();
       ++path_position)
  {
    const auto & segment = path[path_position];
    if (!segment.is_object())
    {
      reject(object_id, "contains a malformed path segment");
    }
    const auto kind = segment.value("kind", "");
    if (kind == "field")
    {
      select_field(structures, segment, object_id, state, offset, plan);
      continue;
    }
    if (kind != "index" || index_position >= indices.size() ||
        !segment.contains("value") || !segment["value"].is_string() ||
        !node["indices"][index_position].is_string() ||
        segment["value"] != node["indices"][index_position])
    {
      reject(object_id, "contains an invalid index transition");
    }
    const auto position = index_position++;
    const auto index = numeric_index(indices[position]);
    if (!index || *index < 0)
    {
      reject(object_id, "index is not an exact non-negative integer");
    }
    select_index(*index, object_id, state, offset, position, plan);
  }

  if (index_position != indices.size() || !state.shape.empty())
  {
    reject(object_id, "path does not reach one leaf access");
  }
  std::int64_t end = 0;
  if (__builtin_add_overflow(offset, state.element_size, &end) ||
      end > object_extent)
  {
    reject(object_id, "exceeds the object extent");
  }
  if (plan) plan->width = state.element_size;
  return {offset, state.element_size};
}

}  // namespace

ByteAccess resolve_structured_access(const nlohmann::json & node,
                                     const std::vector<std::string> & indices,
                                     const nlohmann::json & objects,
                                     const nlohmann::json & structures)
{
  return resolve_path(node, indices, objects, structures, nullptr);
}

ByteAccess prepare_structured_access(const nlohmann::json & node,
                                     const std::vector<std::int64_t> & indices,
                                     const nlohmann::json & objects,
                                     const nlohmann::json & structures,
                                     PreparedLayout & plan)
{
  return resolve_path(node, indices, objects, structures, &plan);
}

}  // namespace yarda::detail
