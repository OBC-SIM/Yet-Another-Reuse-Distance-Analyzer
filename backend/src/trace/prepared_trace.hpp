#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

#include "expansion_budget.hpp"

namespace yarda::detail
{

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

}  // namespace yarda::detail
