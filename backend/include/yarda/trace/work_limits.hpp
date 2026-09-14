#pragma once

#include <cstdint>

namespace yarda
{

/**
 * @brief Inclusive loop-work limits for one streaming module invocation.
 *
 * Zero allows no iterations, including in loops with empty bodies. Zero-trip
 * loops and accesses outside loops remain allowed. Limits are finite uint64_t
 * allowances; zero is never an unlimited sentinel. Task boundaries do not
 * reset cumulative work. Structural node/depth and emission limits are
 * separate.
 */
struct LoopWorkLimits
{
  /** @brief Maximum trip count at each dynamic entry into an individual loop.
   */
  std::uint64_t single_loop_iterations = 1'000'000;
  /**
   * @brief Maximum sum of trip counts reserved across all tasks.
   *
   * Each entered loop reserves its entire trip count before executing its
   * body. Nested loops reserve again at each dynamic entry, even when empty.
   */
  std::uint64_t cumulative_loop_iterations = 1'000'000;
};

}  // namespace yarda
