#include "access_layout_structured.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

struct TypeState
{
  std::string kind;
  std::vector<std::int64_t> shape;
  std::string element_type;
  std::int64_t element_size;
};

[[noreturn]] void reject(const std::string & object_id,
                         const std::string & reason)
{
  throw std::invalid_argument("structured access " + reason + ": " + object_id);
}

std::int64_t required_integer(const Json & value, const char * key,
                              const std::string & object_id)
{
  if (!value.contains(key) || !value[key].is_number_integer())
  {
    reject(object_id, std::string("lacks integer ") + key);
  }
  return value[key].get<std::int64_t>();
}

TypeState type_state(const Json & metadata, const std::string & object_id)
{
  if (!metadata.is_object())
  {
    reject(object_id, "has invalid type metadata");
  }
  TypeState state;
  state.kind = metadata.value("kind", "");
  state.element_type = metadata.value("elem_type", "");
  state.element_size = required_integer(metadata, "elem_size", object_id);
  if (state.element_size <= 0)
  {
    reject(object_id, "has non-positive element size");
  }
  if (!metadata.contains("shape"))
  {
    return state;
  }
  if (!metadata["shape"].is_array())
  {
    reject(object_id, "has invalid shape");
  }
  for (const auto & dimension : metadata["shape"])
  {
    if (!dimension.is_number_integer() || dimension.get<std::int64_t>() <= 0)
    {
      reject(object_id, "has invalid dimension");
    }
    state.shape.push_back(dimension.get<std::int64_t>());
  }
  return state;
}

std::int64_t type_extent(const TypeState & state, const std::string & object_id)
{
  std::int64_t extent = state.element_size;
  for (const auto dimension : state.shape)
  {
    if (__builtin_mul_overflow(extent, dimension, &extent))
    {
      reject(object_id, "type extent overflows");
    }
  }
  return extent;
}

const Json & find_field(const Json & structures, const TypeState & state,
                        const Json & segment, const std::string & object_id)
{
  if (!state.shape.empty() || state.element_type.empty() ||
      !structures.is_object() || !structures.contains(state.element_type))
  {
    reject(object_id, "field transition lacks structure layout");
  }
  const auto & structure = structures.at(state.element_type);
  if (!structure.is_object() || !structure.contains("fields") ||
      !structure["fields"].is_array())
  {
    reject(object_id, "has malformed structure layout");
  }
  if (required_integer(structure, "size", object_id) != state.element_size)
  {
    reject(object_id, "structure size disagrees with its type");
  }
  const auto wanted = required_integer(segment, "index", object_id);
  for (const auto & field : structure["fields"])
  {
    if (!field.is_object() || !field.contains("index") ||
        !field["index"].is_number_integer() ||
        field["index"].get<std::int64_t>() != wanted)
    {
      continue;
    }
    if (!segment.contains("name") || !segment["name"].is_string() ||
        !field.contains("name") || !field["name"].is_string() ||
        segment["name"].get<std::string>() != field["name"].get<std::string>())
    {
      reject(object_id, "field name and index disagree");
    }
    return field;
  }
  reject(object_id, "references an unknown field index");
}

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
                  std::int64_t & offset)
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
}

void select_index(std::int64_t index, const std::string & object_id,
                  TypeState & state, std::int64_t & offset)
{
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
}

}  // namespace

ByteAccess resolve_structured_access(const nlohmann::json & node,
                                     const std::vector<std::string> & indices,
                                     const nlohmann::json & objects,
                                     const nlohmann::json & structures)
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
      select_field(structures, segment, object_id, state, offset);
      continue;
    }
    if (kind != "index" || index_position >= indices.size() ||
        !segment.contains("value") || !segment["value"].is_string() ||
        !node["indices"][index_position].is_string() ||
        segment["value"] != node["indices"][index_position])
    {
      reject(object_id, "contains an invalid index transition");
    }
    const auto index = parse_exact_integer(indices[index_position++]);
    if (!index || *index < 0)
    {
      reject(object_id, "index is not an exact non-negative integer");
    }
    select_index(*index, object_id, state, offset);
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
  return {offset, state.element_size};
}

}  // namespace yarda::detail
