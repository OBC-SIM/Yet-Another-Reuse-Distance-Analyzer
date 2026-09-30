#include "instruction_model.hpp"

#include <limits>
#include <set>
#include <stdexcept>

namespace yarda::detail::ir
{
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

FunctionCounts read_function(const Json & function)
{
  const auto name = function.at("function").get<std::string>();
  if (!function.contains("ir_instructions"))
    throw std::invalid_argument(name +
                                ": missing ir_instructions; regenerate MAP");
  const auto & model = function.at("ir_instructions");
  if (!model.is_object() || !model.contains("version") ||
      (integer(model.at("version")) != 1 && integer(model.at("version")) != 2))
    throw std::invalid_argument(name + ": unsupported IR count version");
  if (model.value("status", "") != "exact")
    throw std::invalid_argument(name + ": " +
                                model.value("reason", "IR count is not exact"));
  const bool legacy = integer(model.at("version")) == 1;
  const auto * exclusions = legacy ? "debug-and-lifetime-intrinsics"
                                   : "phi-debug-and-lifetime-intrinsics";
  if (function.contains("analysis_scope") ||
      model.value("scope", "") != "function-exclusive" ||
      model.value("basis", "") != "map-extraction-ir" ||
      model.value("excluded", "") != exclusions)
    throw std::invalid_argument(name + ": unsupported IR counting contract");
  const auto & blocks = model.at("blocks");
  if (!blocks.is_array() || blocks.empty())
    throw std::invalid_argument(name + ": expected IR blocks");
  std::set<std::uint64_t> ids;
  std::vector<Call> calls;
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
      if (opcode == "phi") continue;
      const auto dynamic = multiply(stat, executions);
      accumulate(detail, opcode, stat, dynamic);
      accumulate(result, opcode, stat, dynamic);
    }
    std::uint64_t call_count = 0;
    for (const auto * opcode : {"call", "invoke", "callbr"})
      if (opcodes.contains(opcode))
        call_count = add(call_count, integer(opcodes.at(opcode)));
    if (legacy && call_count)
      throw std::invalid_argument(name +
                                  ": missing IR call targets; regenerate MAP");
    if (!legacy)
    {
      const auto & sites = block.at("calls");
      if (!sites.is_array() || sites.size() != call_count)
        throw std::invalid_argument(name +
                                    ": IR call metadata does not match "
                                    "opcodes");
      for (const auto & site : sites)
      {
        const auto & target = site.at("callee");
        if (target.is_null())
          throw std::invalid_argument(name +
                                      ": indirect IR call is unsupported");
        calls.push_back({target.get<std::string>(),
                         site.at("inline").get<bool>(), executions});
      }
    }
    result["blocks"].push_back(std::move(detail));
  }
  return {std::move(result), std::move(calls)};
}

}  // namespace yarda::detail::ir
