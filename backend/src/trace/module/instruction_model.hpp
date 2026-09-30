#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace yarda::detail::ir
{
using Json = nlohmann::json;

struct Call
{
  std::string callee;
  bool expand;
  std::uint64_t executions;
};

struct FunctionCounts
{
  Json report;
  std::vector<Call> calls;
};

std::uint64_t integer(const Json & value);
std::uint64_t add(std::uint64_t left, std::uint64_t right);
std::uint64_t multiply(std::uint64_t left, std::uint64_t right);
Json empty_counts();
void accumulate(Json & target, const std::string & opcode, std::uint64_t stat,
                std::uint64_t dynamic);
FunctionCounts read_function(const Json & function);
}  // namespace yarda::detail::ir
