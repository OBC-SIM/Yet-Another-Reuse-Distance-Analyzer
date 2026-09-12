#include "access_layout_structured_type.hpp"

#include <stdexcept>

namespace yarda::detail::structured
{
using Json = nlohmann::json;

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

}  // namespace yarda::detail::structured
