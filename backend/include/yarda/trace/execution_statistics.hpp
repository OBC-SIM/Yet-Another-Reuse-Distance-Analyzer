#pragma once

#include <cstdint>

namespace yarda
{

/**
 * @brief Own producer measurements from a completely successful invocation.
 *
 * Inline depth is structural expansion depth, with analyzed roots at zero;
 * calls in zero-trip loops can contribute. Loop work is the sum of trip counts
 * reserved at dynamic loop entry, including empty and repeated inner loops.
 * Task boundaries do not reset either module-wide observation.
 */
struct TraceExecutionStatistics
{
  std::uint64_t maximum_inline_depth = 0;
  std::uint64_t loop_iterations_expanded = 0;
};

} // namespace yarda
