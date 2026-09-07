#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace yarda::detail
{

/**
 * @brief Add counts or slots within a checked inclusive bound.
 * @param left First non-negative operand.
 * @param right Second non-negative operand.
 * @param limit Largest representable result; small bounds exercise overflow.
 * @return Exact sum.
 * @throws std::overflow_error if either operand or their sum exceeds limit.
 */
inline std::uint64_t csrd_checked_add(
  std::uint64_t left, std::uint64_t right,
  std::uint64_t limit = std::numeric_limits<std::uint64_t>::max())
{
  if (left > limit || right > limit - left)
  {
    throw std::overflow_error("exact CSRD counter or capacity overflows");
  }
  return left + right;
}

/**
 * @brief Convert a storage element count within platform and container bounds.
 * @param count Required elements, including any sentinel element.
 * @param max_size Container's inclusive maximum element count.
 * @return Representable size_t count.
 * @throws std::overflow_error if the count is not representable or too large.
 */
inline std::size_t csrd_storage_size(std::uint64_t count, std::size_t max_size)
{
  if constexpr (std::numeric_limits<std::size_t>::digits <
                std::numeric_limits<std::uint64_t>::digits)
  {
    if (count > std::numeric_limits<std::size_t>::max())
    {
      throw std::overflow_error("exact CSRD storage size overflows size_t");
    }
  }
  const auto size = static_cast<std::size_t>(count);
  if (size > max_size)
  {
    throw std::overflow_error("exact CSRD storage exceeds container maximum");
  }
  return size;
}

}  // namespace yarda::detail
