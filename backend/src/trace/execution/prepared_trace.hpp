#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "expansion_budget.hpp"

namespace yarda::detail
{

struct PreparedAccess;

/** @brief Synchronous consumer; node state and loop slots expire on return. */
using PreparedNodeSink = std::function<void(
  const nlohmann::json &, PreparedAccess &, const std::vector<std::int64_t> &)>;

/** @brief Synchronous consumer; node and indices are borrowed until return. */
using PreparedAccessSink =
  std::function<void(const nlohmann::json &, const std::vector<std::string> &)>;

/**
 * @brief Prepare reached LAT nodes once and emit accesses in execution order.
 *
 * Preparation and loop slots live only for this traversal. Body preparation
 * follows loop reservation and never visits a zero-trip body. Source charging
 * and resolution remain the consumer's responsibility.
 *
 * @param node Borrowed expanded subtree, unchanged throughout traversal.
 * @param budget Borrowed module budget; loop work is charged on every entry.
 * @param sink Required synchronous consumer; must not mutate the input.
 * @return Nothing; no access history is retained after delivery.
 * @throws std::invalid_argument for an executed invalid loop/node or budget.
 * @throws nlohmann::json::exception for malformed fields at their visit site.
 * @note Consumer exceptions propagate unchanged and stop all later visits.
 */
void visit_prepared_trace(const nlohmann::json & node, ExpansionBudget & budget,
                          const PreparedAccessSink & sink);

/**
 * @brief Visit reached nodes with traversal-owned state and current loop slots.
 * @param node Borrowed immutable expanded subtree.
 * @param budget Borrowed module budget; loop work is charged on each entry.
 * @param sink Required synchronous consumer; owns source charge and resolution.
 * @return Nothing; plans are destroyed on normal return and exceptions.
 * @note Uses the same lazy visits and exception order as visit_prepared_trace.
 */
void visit_prepared_accesses(const nlohmann::json & node,
                             ExpansionBudget & budget,
                             const PreparedNodeSink & sink);

}  // namespace yarda::detail
