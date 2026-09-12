#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "yarda/trace/work_limits.hpp"

namespace yarda::detail
{

struct ExpansionLimits
{
  std::uint64_t expanded_nodes;
  std::uint64_t inline_call_depth;
};

inline constexpr ExpansionLimits kExpansionLimits{100'000, 256};

/** Bound structural expansion and dynamic loop work for one LAT module. */
class ExpansionBudget
{
public:
  explicit ExpansionBudget(ExpansionLimits limits = kExpansionLimits,
                           LoopWorkLimits loop_limits = {})
    : limits_(limits), loop_limits_(loop_limits)
  {
  }

  void consume_expanded_node()
  {
    consume(expanded_nodes_, 1, limits_.expanded_nodes, "inline call expansion",
            " nodes");
  }

  void consume_loop_iterations(std::uint64_t count)
  {
    if (count > loop_limits_.single_loop_iterations)
    {
      throw std::invalid_argument(
        "loop iteration count exceeds " +
        std::to_string(loop_limits_.single_loop_iterations));
    }
    consume(loop_iterations_, count, loop_limits_.cumulative_loop_iterations,
            "cumulative loop iteration count", "");
  }

  void validate_inline_call_depth(std::uint64_t depth) const
  {
    if (depth > limits_.inline_call_depth)
    {
      throw std::invalid_argument("inline call depth exceeds " +
                                  std::to_string(limits_.inline_call_depth));
    }
  }

private:
  static void consume(std::uint64_t & used, std::uint64_t amount,
                      std::uint64_t limit, const char * subject,
                      const char * suffix)
  {
    if (used > limit || amount > limit - used)
    {
      throw std::invalid_argument(std::string(subject) + " exceeds " +
                                  std::to_string(limit) + suffix);
    }
    used += amount;
  }

  ExpansionLimits limits_;
  LoopWorkLimits loop_limits_;
  std::uint64_t expanded_nodes_ = 0;
  std::uint64_t loop_iterations_ = 0;
};

}  // namespace yarda::detail
