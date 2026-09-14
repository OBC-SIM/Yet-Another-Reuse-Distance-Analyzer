#include "loop_iteration.hpp"

#include <stdexcept>

namespace yarda::detail
{
namespace
{

std::uint64_t positive_distance(std::int64_t lower, std::int64_t upper)
{
  if (lower < 0 && upper >= 0)
  {
    const auto below_zero =
      static_cast<std::uint64_t>(-(lower + 1)) + std::uint64_t{1};
    return below_zero + static_cast<std::uint64_t>(upper);
  }
  return static_cast<std::uint64_t>(upper - lower);
}

std::uint64_t step_magnitude(std::int64_t step)
{
  return static_cast<std::uint64_t>(-(step + 1)) + std::uint64_t{1};
}

std::uint64_t ceil_divide(std::uint64_t dividend, std::uint64_t divisor)
{
  return dividend / divisor + (dividend % divisor != 0 ? 1 : 0);
}

}  // namespace

std::uint64_t loop_iteration_count(std::int64_t start, std::int64_t bound,
                                   std::int64_t step)
{
  if (step == 0) throw std::invalid_argument("loop step must be non-zero");
  if (step > 0 && start < bound)
    return ceil_divide(positive_distance(start, bound),
                       static_cast<std::uint64_t>(step));
  if (step < 0 && start > bound)
    return ceil_divide(positive_distance(bound, start), step_magnitude(step));
  return 0;
}

}  // namespace yarda::detail
