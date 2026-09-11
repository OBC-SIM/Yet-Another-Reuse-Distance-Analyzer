#pragma once

#include <cstdint>

namespace yarda::detail
{

/**
 * @brief Count an exclusive-bound loop across the full signed value range.
 * @param start First induction value.
 * @param bound Exclusive bound in the direction of step.
 * @param step Signed increment, including INT64_MIN.
 * @return Exact unsigned trip count; a loop pointing away from bound is empty.
 * @throws std::invalid_argument when step is zero.
 */
std::uint64_t loop_iteration_count(std::int64_t start, std::int64_t bound,
                                   std::int64_t step);

}  // namespace yarda::detail
