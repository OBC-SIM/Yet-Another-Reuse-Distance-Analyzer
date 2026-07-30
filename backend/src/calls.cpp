#include "yarda/calls.hpp"

#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "yarda/schema.hpp"

namespace yarda
{
namespace
{

using Json = nlohmann::json;
using Mapping = std::unordered_map<std::string, std::string>;
using Functions = std::unordered_map<std::string, Json>;

const std::regex kAffineName(R"(^([A-Za-z_][A-Za-z0-9_]*)([+-]\d+)?$)");
const std::regex kIdentifier(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)");
const std::unordered_set<std::string> kAnalyze = {"yard.analyze",
                                                  "ape."
                                                  "analyze"};
const std::unordered_set<std::string> kInline = {"yard.inline", "ape.inline"};

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

Json substitute_node(Json node, const Mapping & mapping)
{
  const auto type = node.value("type", "");
  if (type == "Array")
  {
    node["name"] = substitute_text(node.value("name", ""), mapping);
    for (auto & index : node["indices"])
    {
      index = substitute_name(index.get<std::string>(), mapping);
    }
    if (node.contains("access_path") && node["access_path"].is_array())
    {
      for (auto & segment : node["access_path"])
      {
        if (segment.value("kind", "") == "index" && segment.contains("value"))
        {
          segment["value"] =
            substitute_name(segment["value"].get<std::string>(), mapping);
        }
      }
    }
  }
  else if (type == "Scalar")
  {
    node["name"] = substitute_name(node.value("name", ""), mapping);
  }
  else if (type == "Loop")
  {
    for (auto & child : node["body"])
    {
      child = substitute_node(child, mapping);
    }
  }
  else if (type == "Call")
  {
    for (auto & argument : node["args"])
    {
      argument = substitute_name(argument.get<std::string>(), mapping);
    }
  }
  return node;
}

Json expand_body(const Json & body, const Functions & functions,
                 std::unordered_set<std::string> stack)
{
  Json expanded = Json::array();
  for (auto node : body)
  {
    const auto type = node.value("type", "");
    if (type == "Call")
    {
      const auto callee = node.value("callee", "");
      if (!functions.count(callee))
      {
        throw std::invalid_argument("Unknown call target: " + callee);
      }
      if (stack.count(callee))
      {
        throw std::invalid_argument(
          "Recursive call expansion is not supported: " + callee);
      }
      const auto & function = functions.at(callee);
      Mapping mapping;
      const auto params = function.value("params", Json::array());
      const auto args = node.value("args", Json::array());
      for (std::size_t index = 0; index < std::min(params.size(), args.size());
           ++index)
      {
        mapping[params[index].get<std::string>()] =
          args[index].get<std::string>();
      }
      Json substituted = Json::array();
      for (const auto & child : function["body"])
      {
        substituted.push_back(substitute_node(child, mapping));
      }
      stack.insert(callee);
      for (auto & child : expand_body(substituted, functions, stack))
      {
        expanded.push_back(std::move(child));
      }
    }
    else if (type == "Loop")
    {
      node["body"] = expand_body(node["body"], functions, stack);
      expanded.push_back(std::move(node));
    }
    else
    {
      expanded.push_back(std::move(node));
    }
  }
  return expanded;
}

bool has_annotation(const Json & function,
                    const std::unordered_set<std::string> & wanted)
{
  for (const auto & annotation : function.value("annotations", Json::array()))
  {
    if (wanted.count(annotation.get<std::string>()))
    {
      return true;
    }
  }
  return false;
}

}  // namespace

nlohmann::json expand_calls(const nlohmann::json & raw)
{
  auto module = normalize_module(raw);
  Functions functions;
  bool has_roles = false;
  for (const auto & function : module)
  {
    functions[function.at("function").get<std::string>()] = function;
    has_roles = has_roles || has_annotation(function, kAnalyze) ||
                has_annotation(function, kInline);
  }

  Json result = Json::array();
  for (auto function : module)
  {
    const auto name = function.at("function").get<std::string>();
    function["body"] = expand_body(function["body"], functions, {name});
    if (!has_roles || has_annotation(function, kAnalyze))
    {
      result.push_back(std::move(function));
    }
  }
  return result;
}

}  // namespace yarda
