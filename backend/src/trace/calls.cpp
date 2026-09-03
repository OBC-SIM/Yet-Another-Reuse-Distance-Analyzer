#include "yarda/trace/calls.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "call_expansion.hpp"
#include "call_roles.hpp"
#include "call_substitution.hpp"
#include "expansion_budget.hpp"
#include "yarda/trace/schema.hpp"

namespace yarda
{
namespace
{

using Json = nlohmann::json;
using Functions = std::unordered_map<std::string, Json>;

void append_expanded_body(const Json & body, const Functions & functions,
                          const Json & objects,
                          const std::unordered_set<std::string> & stack,
                          bool analyzed_tasks, detail::ExpansionBudget & budget,
                          std::uint64_t inline_depth, Json & expanded);

void append_expanded_node(Json node, const Functions & functions,
                          const Json & objects,
                          const std::unordered_set<std::string> & stack,
                          bool analyzed_tasks, detail::ExpansionBudget & budget,
                          std::uint64_t inline_depth, Json & expanded)
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
      budget.consume_expanded_node();
      expanded.push_back(std::move(node));
      return;
    }
    if (stack.count(callee))
    {
      throw std::invalid_argument(
        "Recursive call expansion is not supported: " + callee);
    }
    const auto next_depth = inline_depth + 1;
    budget.validate_inline_call_depth(next_depth);
    budget.consume_expanded_node();

    const auto & function = target->second;
    detail::CallMapping names;
    detail::CallMapping object_ids;
    const auto params = function.value("params", Json::array());
    const auto args = node.value("args", Json::array());
    const auto arg_objects = node.value("arg_objects", Json::array());
    if (params.size() != args.size())
    {
      throw std::invalid_argument(
        "Call arity mismatch for " + callee + ": expected " +
        std::to_string(params.size()) + ", got " + std::to_string(args.size()));
    }
    if (node.contains("arg_objects") && arg_objects.size() != args.size())
    {
      throw std::invalid_argument("Call object arity mismatch for " + callee +
                                  ": expected " + std::to_string(args.size()) +
                                  ", got " +
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

    auto child_stack = stack;
    child_stack.insert(callee);
    for (const auto & child : function["body"])
    {
      append_expanded_node(
        detail::substitute_call_node(child, names, object_ids, objects),
        functions, objects, child_stack, analyzed_tasks, budget, next_depth,
        expanded);
    }
    return;
  }
  if (type == "Loop")
  {
    Json expanded_body = Json::array();
    append_expanded_body(node["body"], functions, objects, stack,
                         analyzed_tasks, budget, inline_depth, expanded_body);
    node["body"] = std::move(expanded_body);
  }
  budget.consume_expanded_node();
  expanded.push_back(std::move(node));
}

void append_expanded_body(const Json & body, const Functions & functions,
                          const Json & objects,
                          const std::unordered_set<std::string> & stack,
                          bool analyzed_tasks, detail::ExpansionBudget & budget,
                          std::uint64_t inline_depth, Json & expanded)
{
  for (const auto & node : body)
  {
    append_expanded_node(node, functions, objects, stack, analyzed_tasks,
                         budget, inline_depth, expanded);
  }
}

Json expand_module(const nlohmann::json & raw, bool analyzed_tasks,
                   detail::ExpansionBudget & budget)
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
    Json expanded_body = Json::array();
    append_expanded_body(function["body"], functions, objects, {name},
                         analyzed_tasks, budget, 0, expanded_body);
    function["body"] = std::move(expanded_body);
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

namespace detail
{

nlohmann::json expand_calls(const nlohmann::json & raw,
                            ExpansionBudget & budget)
{
  return expand_module(raw, false, budget);
}

nlohmann::json expand_task_calls(const nlohmann::json & raw,
                                 ExpansionBudget & budget)
{
  return expand_module(raw, true, budget);
}

}  // namespace detail

nlohmann::json expand_calls(const nlohmann::json & raw)
{
  detail::ExpansionBudget budget;
  return detail::expand_calls(raw, budget);
}

nlohmann::json expand_task_calls(const nlohmann::json & raw)
{
  detail::ExpansionBudget budget;
  return detail::expand_task_calls(raw, budget);
}

}  // namespace yarda
