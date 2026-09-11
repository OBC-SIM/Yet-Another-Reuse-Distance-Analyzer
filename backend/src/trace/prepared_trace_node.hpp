#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "prepared_index.hpp"

namespace yarda::detail
{

struct PreparedNode;

/** @brief Own index plans and one reusable output row, never access history. */
struct PreparedAccess
{
  std::vector<PreparedIndex> indices;
  std::vector<std::string> values;
};

/** @brief Keep one lexical loop and its lazily reached body for a traversal. */
struct PreparedLoop
{
  std::string variable;
  std::int64_t start = 0;
  std::int64_t step = 1;
  std::uint64_t count = 0;
  std::size_t slot = 0;
  bool body_prepared = false;
  std::vector<PreparedNode> body;
};

/**
 * @brief Borrow an immutable LAT node until its first execution prepares it.
 *
 * A pending child holds no parsed fields, so a previous callback can stop
 * traversal before malformed child fields are observed. The input subtree
 * outlives this node and all its descendants.
 */
struct PreparedNode
{
  const nlohmann::json * source;
  std::variant<std::monostate, PreparedAccess, PreparedLoop> payload;

  /**
   * @brief Defer inspection of a borrowed node until it is reached.
   * @param node Non-null immutable LAT reference; ownership stays with caller.
   */
  explicit PreparedNode(const nlohmann::json & node) : source(&node) {}
};

}  // namespace yarda::detail
