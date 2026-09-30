#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace yarda::detail
{

/**
 * @brief Derive a task label without changing its function/object binding.
 * @param function Borrowed normalized function with a validated scope.
 * @return Original function name or length-prefixed region identity.
 */
std::string analysis_task_id(const nlohmann::json & function);

/**
 * @brief Validate all scopes and selected identities before expansion/delivery.
 * @param functions Borrowed normalized module; no entries are modified.
 * @param task_mode True for selected-task consumers; legacy rejects scopes.
 * @return Nothing on success.
 * @throws std::invalid_argument for unsupported scopes or task ID collisions.
 */
void validate_task_scopes(const nlohmann::json & functions, bool task_mode);

}  // namespace yarda::detail
