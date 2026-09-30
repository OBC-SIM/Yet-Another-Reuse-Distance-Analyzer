#include "yarda/trace/instruction_counts.hpp"

#include <map>
#include <set>
#include <stdexcept>

#include "call_roles.hpp"
#include "instruction_model.hpp"
#include "yarda/trace/schema.hpp"

namespace yarda
{
namespace
{
using namespace detail::ir;

void include(Json & target, const Json & source, std::uint64_t executions)
{
  for (const auto & [opcode, count] : source.at("opcodes").items())
    accumulate(target, opcode, integer(count.at("static")),
               multiply(integer(count.at("dynamic")), executions));
}

class Expansion
{
public:
  std::map<std::string, const Json *> definitions;

  const Json & count(const std::string & name)
  {
    if (const auto found = completed.find(name); found != completed.end())
      return found->second;
    if (!active.insert(name).second)
      throw std::invalid_argument(name + ": recursive inline IR call");
    auto model = read_function(*definitions.at(name));
    auto & result = model.report;
    auto self = empty_counts();
    include(self, result, 1);
    result["self"] = std::move(self);
    result["inline_callees"] = Json::array();
    for (const auto & call : model.calls)
    {
      const auto found = definitions.find(call.callee);
      const bool inline_definition =
        found != definitions.end() &&
        detail::call_roles::is_inline(*found->second);
      if (call.expand != inline_definition)
        throw std::invalid_argument(
          name +
          ": missing or inconsistent inline IR definition: " + call.callee);
      if (!call.expand) continue;
      const auto & callee = count(call.callee);
      auto contribution = empty_counts();
      include(contribution, callee, call.executions);
      contribution["function"] = call.callee;
      contribution["invocations"] = call.executions;
      include(result, contribution, 1);
      result["inline_callees"].push_back(std::move(contribution));
    }
    active.erase(name);
    return completed.emplace(name, std::move(result)).first->second;
  }

private:
  std::map<std::string, Json> completed;
  std::set<std::string> active;
};
}  // namespace

nlohmann::json count_ir_instructions(const nlohmann::json & raw)
{
  const auto functions = normalize_module(raw);
  Expansion expansion;
  bool annotated = false;
  for (const auto & function : functions)
  {
    const auto name = function.at("function").get<std::string>();
    if (!expansion.definitions.emplace(name, &function).second)
      throw std::invalid_argument("duplicate IR function name: " + name);
    if (detail::call_roles::is_analyzed(function) &&
        detail::call_roles::is_inline(function))
      throw std::invalid_argument(name + ": conflicting IR function roles");
    annotated |= detail::call_roles::is_analyzed(function);
  }
  Json results = Json::array();
  auto total = empty_counts();
  for (const auto & function : functions)
  {
    if (annotated ? !detail::call_roles::is_analyzed(function)
                  : detail::call_roles::is_inline(function))
      continue;
    const auto & result =
      expansion.count(function.at("function").get<std::string>());
    include(total, result, 1);
    results.push_back(result);
  }
  if (results.empty())
    throw std::invalid_argument("no IR instruction analysis roots");
  return {{"schema_version", 2},
          {"analysis", "ir-instructions"},
          {"status", "exact"},
          {"basis", "map-extraction-ir"},
          {"scope", "inline-expanded"},
          {"excluded", "phi-debug-and-lifetime-intrinsics"},
          {"invocations_per_root", 1},
          {"functions", std::move(results)},
          {"total", std::move(total)}};
}

}  // namespace yarda
