#include "call_substitution.hpp"

#include <regex>
#include <string>

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

const std::regex kAffineName(R"(^([A-Za-z_][A-Za-z0-9_]*)([+-]\d+)?$)");
const std::regex kIdentifier(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)");

std::string substitute_name(const std::string & name,
                            const CallMapping & mapping)
{
  if (const auto exact = mapping.find(name); exact != mapping.end())
  {
    return exact->second;
  }
  std::smatch match;
  if (std::regex_match(name, match, kAffineName))
  {
    if (const auto base = mapping.find(match[1].str()); base != mapping.end())
    {
      return base->second + match[2].str();
    }
  }
  return name;
}

std::string substitute_text(const std::string & text,
                            const CallMapping & mapping)
{
  std::string output;
  std::sregex_iterator current(text.begin(), text.end(), kIdentifier);
  const std::sregex_iterator end;
  std::size_t copied = 0;
  for (; current != end; ++current)
  {
    output.append(text, copied,
                  static_cast<std::size_t>(current->position()) - copied);
    const auto token = current->str();
    const auto replacement = mapping.find(token);
    output += replacement == mapping.end() ? token : replacement->second;
    copied = static_cast<std::size_t>(current->position() + current->length());
  }
  output.append(text, copied, std::string::npos);
  return output;
}

void apply_object_metadata(Json & node, const Json & objects)
{
  const auto object_id = node.value("object", "");
  if (object_id.empty() || !objects.contains(object_id))
  {
    return;
  }
  const auto & metadata = objects.at(object_id);
  for (const auto * field : {"shape", "elem_size"})
  {
    if (metadata.contains(field))
    {
      node[field] = metadata.at(field);
    }
  }
}

}  // namespace

nlohmann::json substitute_call_node(nlohmann::json node,
                                    const CallMapping & names,
                                    const CallMapping & object_ids,
                                    const nlohmann::json & objects)
{
  const auto type = node.value("type", "");
  if (type == "Array")
  {
    node["name"] = substitute_text(node.value("name", ""), names);
    if (const auto object = object_ids.find(node.value("object", ""));
        object != object_ids.end())
    {
      node["object"] = object->second;
      apply_object_metadata(node, objects);
    }
    for (auto & index : node["indices"])
    {
      index = substitute_name(index.get<std::string>(), names);
    }
    if (node.contains("access_path") && node["access_path"].is_array())
    {
      for (auto & segment : node["access_path"])
      {
        if (segment.value("kind", "") == "index" && segment.contains("value"))
        {
          segment["value"] =
            substitute_name(segment["value"].get<std::string>(), names);
        }
      }
    }
  }
  else if (type == "Scalar")
  {
    node["name"] = substitute_name(node.value("name", ""), names);
  }
  else if (type == "Loop")
  {
    for (auto & child : node["body"])
    {
      child = substitute_call_node(child, names, object_ids, objects);
    }
  }
  else if (type == "Call")
  {
    for (auto & argument : node["args"])
    {
      argument = substitute_name(argument.get<std::string>(), names);
    }
    if (node.contains("arg_objects") && node["arg_objects"].is_array())
    {
      for (auto & object : node["arg_objects"])
      {
        if (const auto actual = object_ids.find(object.get<std::string>());
            actual != object_ids.end())
        {
          object = actual->second;
        }
      }
    }
  }
  return node;
}

}  // namespace yarda::detail
