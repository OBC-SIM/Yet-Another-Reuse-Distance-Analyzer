#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace yarda::detail
{

struct ExpansionLimits
{
  std::uint64_t expanded_nodes;
  std::uint64_t loop_iterations;
  std::uint64_t inline_call_depth;
};

inline constexpr ExpansionLimits kExpansionLimits{100'000, 1'000'000, 256};

/** Bound structural expansion work for one LAT module. */
class ExpansionBudget
{
public:
  explicit ExpansionBudget(ExpansionLimits limits = kExpansionLimits)
    : limits_(limits)
  {
  }

  void consume_expanded_node()
  {
    consume(expanded_nodes_, 1, limits_.expanded_nodes, "inline call expansion",
            " nodes");
  }

  void consume_loop_iterations(std::uint64_t count)
  {
    consume(loop_iterations_, count, limits_.loop_iterations,
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
  std::uint64_t expanded_nodes_ = 0;
  std::uint64_t loop_iterations_ = 0;
};

}  // namespace yarda::detail
