#include "yarda/trace/instruction_counts.hpp"

#include <limits>
#include <set>
#include <stdexcept>

#include "call_roles.hpp"
#include "yarda/trace/schema.hpp"

namespace yarda
{
namespace
{
using Json = nlohmann::json;

std::uint64_t integer(const Json & value)
{
  if (!value.is_number_integer() ||
      (!value.is_number_unsigned() && value.get<std::int64_t>() < 0))
    throw std::invalid_argument(
      "IR counts require nonnegative uint64 integers");
  return value.get<std::uint64_t>();
}

std::uint64_t add(std::uint64_t left, std::uint64_t right)
{
  if (left > std::numeric_limits<std::uint64_t>::max() - right)
    throw std::overflow_error("IR instruction count exceeds uint64");
  return left + right;
}

std::uint64_t multiply(std::uint64_t left, std::uint64_t right)
{
  if (right && left > std::numeric_limits<std::uint64_t>::max() / right)
    throw std::overflow_error("IR instruction count exceeds uint64");
  return left * right;
}

Json empty_counts()
{
  return {{"static_instructions", 0},
          {"dynamic_instructions", 0},
          {"opcodes", Json::object()}};
}

void accumulate(Json & target, const std::string & opcode, std::uint64_t stat,
                std::uint64_t dynamic)
{
  auto & entry = target["opcodes"][opcode];
  if (entry.is_null()) entry = {{"static", 0}, {"dynamic", 0}};
  entry["static"] = add(integer(entry["static"]), stat);
  entry["dynamic"] = add(integer(entry["dynamic"]), dynamic);
  target["static_instructions"] =
    add(integer(target["static_instructions"]), stat);
  target["dynamic_instructions"] =
    add(integer(target["dynamic_instructions"]), dynamic);
}

Json count_function(const Json & function)
{
  const auto name = function.at("function").get<std::string>();
  if (!function.contains("ir_instructions"))
    throw std::invalid_argument(name +
                                ": missing ir_instructions; regenerate MAP");
  const auto & model = function.at("ir_instructions");
  if (!model.is_object() || !model.contains("version") ||
      integer(model.at("version")) != 1)
    throw std::invalid_argument(name + ": unsupported IR count version");
  if (model.value("status", "") != "exact")
    throw std::invalid_argument(name + ": " +
                                model.value("reason", "IR count is not exact"));
  if (function.contains("analysis_scope") ||
      model.value("scope", "") != "function-exclusive" ||
      model.value("basis", "") != "map-extraction-ir" ||
      model.value("excluded", "") != "debug-and-lifetime-intrinsics")
    throw std::invalid_argument(name + ": unsupported IR counting contract");
  const auto & blocks = model.at("blocks");
  if (!blocks.is_array() || blocks.empty())
    throw std::invalid_argument(name + ": expected IR blocks");
  std::set<std::uint64_t> ids;
  auto result = empty_counts();
  result["function"] = name;
  result["blocks"] = Json::array();
  for (const auto & block : blocks)
  {
    const auto id = integer(block.at("id"));
    if (!ids.insert(id).second)
      throw std::invalid_argument(name + ": duplicate IR block id");
    const auto executions = integer(block.at("executions"));
    const auto & opcodes = block.at("opcodes");
    if (!opcodes.is_object())
      throw std::invalid_argument(name + ": expected IR opcode counts");
    auto detail = empty_counts();
    detail["id"] = id;
    detail["name"] = block.at("name").get<std::string>();
    detail["executions"] = executions;
    for (const auto & [opcode, count] : opcodes.items())
    {
      const auto stat = integer(count);
      const auto dynamic = multiply(stat, executions);
      accumulate(detail, opcode, stat, dynamic);
      accumulate(result, opcode, stat, dynamic);
    }
    result["blocks"].push_back(std::move(detail));
  }
  return result;
}

}  // namespace

nlohmann::json count_ir_instructions(const nlohmann::json & raw)
{
  const auto functions = normalize_module(raw);
  bool annotated = false;
  for (const auto & function : functions)
    annotated |= detail::call_roles::is_analyzed(function);
  Json results = Json::array();
  auto total = empty_counts();
  std::set<std::string> names;
  for (const auto & function : functions)
  {
    if (annotated ? !detail::call_roles::is_analyzed(function)
                  : detail::call_roles::is_inline(function))
      continue;
    const auto result = count_function(function);
    if (!names.insert(result.at("function").get<std::string>()).second)
      throw std::invalid_argument("duplicate IR function name");
    for (const auto & [opcode, count] : result.at("opcodes").items())
      accumulate(total, opcode, integer(count.at("static")),
                 integer(count.at("dynamic")));
    results.push_back(result);
  }
  if (results.empty())
    throw std::invalid_argument("no IR instruction analysis roots");
  return {{"schema_version", 1},
          {"analysis", "ir-instructions"},
          {"basis", "map-extraction-ir"},
          {"scope", "function-exclusive"},
          {"excluded", "debug-and-lifetime-intrinsics"},
          {"invocations_per_root", 1},
          {"functions", std::move(results)},
          {"total", std::move(total)}};
}

}  // namespace yarda
