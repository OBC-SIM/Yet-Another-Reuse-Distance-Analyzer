#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "access_layout.hpp"

namespace yarda::detail
{

/** @brief One ordered index multiplication or constant field displacement. */
struct PreparedLayoutStep
{
  std::optional<std::size_t> index;
  std::int64_t stride;
  std::int64_t dimension = 0;
};

/** @brief Value-owned layout recorded during a successful first access. */
struct PreparedLayout
{
  bool structured = false;
  std::int64_t width = 0;
  std::int64_t extent = 0;
  std::vector<PreparedLayoutStep> steps;

  /**
   * @brief Evaluate current indices in the original checked arithmetic order.
   * @param indices Borrowed numeric row with the rank used during preparation.
   * @param object_id Borrowed diagnostic identity for structured accesses.
   * @return Byte range, or no value on legacy arithmetic overflow.
   * @throws std::invalid_argument for an invalid structured dynamic range.
   * @pre This plan was produced by a successful layout preparation.
   */
  std::optional<ByteAccess> resolve(const std::vector<std::int64_t> & indices,
                                    const std::string & object_id) const;
};

}  // namespace yarda::detail
