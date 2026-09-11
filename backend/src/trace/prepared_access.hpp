#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "prepared_index.hpp"
#include "prepared_layout.hpp"
#include "yarda/access_operation.hpp"
#include "yarda/elf/address_model.hpp"

namespace yarda::detail
{

/**
 * @brief Own immutable binding facts from a successfully resolved access.
 *
 * This plan belongs to one expanded access node in one traversal. It owns all
 * metadata and never retains input/model pointers or a dynamic resolved range.
 */
struct PreparedAccessPlan
{
  std::string object_id;
  AccessOperation operation = AccessOperation::Unknown;
  ObjectAddress object;
  AddressBasis basis = AddressBasis::Absolute;
  PreparedLayout layout;
};

/** @brief Own index plans and reusable rows, without dynamic access history. */
struct PreparedAccess
{
  std::vector<PreparedIndex> indices;
  std::vector<std::string> values;
  std::vector<std::int64_t> numeric_values;
  std::optional<PreparedAccessPlan> plan;

  /**
   * @brief Update a reusable numeric row without raising semantic errors.
   * @param slots Borrowed current lexical loop values; never retained.
   * @return Whether every index is exact; false is rejected after source charge
   * and any earlier operation checks, using the existing resolution contract.
   */
  bool evaluate_numeric(const std::vector<std::int64_t> & slots);
};

}  // namespace yarda::detail
