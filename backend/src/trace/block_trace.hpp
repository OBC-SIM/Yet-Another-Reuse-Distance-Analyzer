#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

#include "yarda/trace/calls.hpp"

namespace yarda::detail
{

enum class EmptyLoopPolicy
{
  Include,
  Omit,
};

template<typename Named, typename Access, typename Unroll>
std::vector<Named> build_block_traces(const nlohmann::json & raw,
                                      const Unroll & unroll,
                                      EmptyLoopPolicy empty_loop_policy)
{
  const auto module = expand_calls(raw);
  std::vector<Named> result;
  for (const auto & function : module)
  {
    const auto function_name = function.at("function").get<std::string>();
    std::vector<Access> flat;
    const auto flush_flat = [&]() {
      if (!flat.empty())
      {
        const auto name = function_name + "  (flat, " +
                          std::to_string(flat.size()) + " accesses)";
        result.push_back({name, std::move(flat)});
        flat.clear();
      }
    };
    for (const auto & node :
         function.value("body", nlohmann::json::array()))
    {
      auto accesses = unroll(node);
      if (node.value("type", "") == "Loop")
      {
        flush_flat();
        const auto name =
          function_name + "  " + node.value("var", "") +
          "-loop (bound=" + std::to_string(node.value("bound", 0)) + ")";
        if (empty_loop_policy == EmptyLoopPolicy::Include || !accesses.empty())
        {
          result.push_back({name, std::move(accesses)});
        }
      }
      else
      {
        flat.insert(flat.end(), accesses.begin(), accesses.end());
      }
    }
    flush_flat();
  }
  return result;
}

}  // namespace yarda::detail
