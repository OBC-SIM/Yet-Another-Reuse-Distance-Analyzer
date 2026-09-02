#include "access_layout.hpp"

#include <exception>
#include <string>

#include "access_layout_legacy.hpp"
#include "access_layout_structured.hpp"

namespace yarda::detail
{
using Json = nlohmann::json;

bool has_field_path(const Json & node)
{
  if (!node.contains("access_path") || !node["access_path"].is_array())
  {
    return false;
  }
  for (const auto & segment : node["access_path"])
  {
    if (segment.is_object() && segment.value("kind", "") == "field")
    {
      return true;
    }
  }
  return false;
}

std::optional<std::int64_t> parse_exact_integer(const std::string & value)
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

AccessLayoutResolver::AccessLayoutResolver()
  : objects_(Json::object()), structs_(Json::object())
{
}

AccessLayoutResolver::AccessLayoutResolver(const nlohmann::json & raw)
  : AccessLayoutResolver()
{
  if (raw.is_object() && raw.contains("metadata") &&
      raw["metadata"].is_object())
  {
    objects_ = raw["metadata"].value("objects", Json::object());
    structs_ = raw["metadata"].value("structs", Json::object());
  }
}

std::optional<ByteAccess> AccessLayoutResolver::resolve(
  const nlohmann::json & node, const std::vector<std::string> & indices) const
{
  if (!has_field_path(node))
  {
    return resolve_legacy_access(node, indices);
  }
  return resolve_structured_access(node, indices, objects_, structs_);
}

std::optional<std::string>
AccessLayoutResolver::object_kind(const std::string & object_id) const
{
  if (!objects_.is_object() || !objects_.contains(object_id) ||
      !objects_.at(object_id).is_object())
  {
    return std::nullopt;
  }
  const auto & metadata = objects_.at(object_id);
  return metadata.contains("kind") && metadata.at("kind").is_string()
           ? std::optional<std::string>(metadata.at("kind").get<std::string>())
           : std::optional<std::string>(std::string{});
}

}  // namespace yarda::detail
