#include "yarda/trace/calls.hpp"

#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "call_roles.hpp"
#include "yarda/trace/schema.hpp"

namespace yarda
{
namespace
{

using Json = nlohmann::json;
using Mapping = std::unordered_map<std::string, std::string>;
using Functions = std::unordered_map<std::string, Json>;

const std::regex kAffineName(R"(^([A-Za-z_][A-Za-z0-9_]*)([+-]\d+)?$)");
const std::regex kIdentifier(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)");

std::string substitute_name(const std::string & name, const Mapping & mapping)
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

std::string substitute_text(const std::string & text, const Mapping & mapping)
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

Json substitute_node(Json node, const Mapping & names,
                     const Mapping & object_ids, const Json & objects)
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
      child = substitute_node(child, names, object_ids, objects);
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

Json expand_body(const Json & body, const Functions & functions,
                 const Json & objects, std::unordered_set<std::string> stack,
                 bool analyzed_tasks)
{
  Json expanded = Json::array();
  for (auto node : body)
  {
    const auto type = node.value("type", "");
    if (type == "Call")
    {
      const auto callee = node.value("callee", "");
      const auto target = functions.find(callee);
      if (target == functions.end())
      {
        throw std::invalid_argument("Unknown call target: " + callee);
      }
      if (analyzed_tasks && !detail::call_roles::is_inline(target->second))
      {
        continue;
      }
      if (stack.count(callee))
      {
        throw std::invalid_argument(
          "Recursive call expansion is not supported: " + callee);
      }
      const auto & function = target->second;
      Mapping names;
      Mapping object_ids;
      const auto params = function.value("params", Json::array());
      const auto args = node.value("args", Json::array());
      const auto arg_objects = node.value("arg_objects", Json::array());
      if (params.size() != args.size())
      {
        throw std::invalid_argument("Call arity mismatch for " + callee +
                                    ": expected " +
                                    std::to_string(params.size()) + ", got " +
                                    std::to_string(args.size()));
      }
      if (node.contains("arg_objects") && arg_objects.size() != args.size())
      {
        throw std::invalid_argument("Call object arity mismatch for " + callee +
                                    ": expected " +
                                    std::to_string(args.size()) + ", got " +
                                    std::to_string(arg_objects.size()));
      }
      for (std::size_t index = 0; index < params.size(); ++index)
      {
        const auto parameter = params[index].get<std::string>();
        names[parameter] = args[index].get<std::string>();
        if (index < arg_objects.size())
        {
          object_ids["function:" + callee + "::param:" + parameter] =
            arg_objects[index].get<std::string>();
        }
      }
      Json substituted = Json::array();
      for (const auto & child : function["body"])
      {
        substituted.push_back(
          substitute_node(child, names, object_ids, objects));
      }
      stack.insert(callee);
      for (auto & child :
           expand_body(substituted, functions, objects, stack, analyzed_tasks))
      {
        expanded.push_back(std::move(child));
      }
    }
    else if (type == "Loop")
    {
      node["body"] =
        expand_body(node["body"], functions, objects, stack, analyzed_tasks);
      expanded.push_back(std::move(node));
    }
    else
    {
      expanded.push_back(std::move(node));
    }
  }
  return expanded;
}

Json expand_module(const nlohmann::json & raw, bool analyzed_tasks)
{
  Json objects = Json::object();
  if (raw.is_object() && raw.contains("metadata") &&
      raw["metadata"].is_object())
  {
    objects = raw["metadata"].value("objects", Json::object());
  }
  auto module = normalize_module(raw);
  Functions functions;
  bool has_roles = false;
  for (const auto & function : module)
  {
    const auto name = function.at("function").get<std::string>();
    if (name.empty())
    {
      throw std::invalid_argument("Function identity must not be empty");
    }
    if (!functions.emplace(name, function).second)
    {
      throw std::invalid_argument("Duplicate function identity: " + name);
    }
    const bool analyzed = detail::call_roles::is_analyzed(function);
    const bool inlined = detail::call_roles::is_inline(function);
    if (analyzed_tasks && analyzed && inlined)
    {
      throw std::invalid_argument(
        "Function is both an analyzed root and inline: " + name);
    }
    has_roles = has_roles || analyzed || inlined;
  }

  Json result = Json::array();
  for (auto function : module)
  {
    const auto name = function.at("function").get<std::string>();
    const bool selected = detail::call_roles::is_analyzed(function) ||
                          (!analyzed_tasks && !has_roles);
    if (analyzed_tasks && !selected)
    {
      continue;
    }
    function["body"] =
      expand_body(function["body"], functions, objects, {name}, analyzed_tasks);
    if (selected)
    {
      result.push_back(std::move(function));
    }
  }
  if (analyzed_tasks && result.empty())
  {
    throw std::invalid_argument("LAT module contains no analyzed task root");
  }
  return result;
}

}  // namespace

nlohmann::json expand_calls(const nlohmann::json & raw)
{
  return expand_module(raw, false);
}

nlohmann::json expand_task_calls(const nlohmann::json & raw)
{
  return expand_module(raw, true);
}

}  // namespace yarda
