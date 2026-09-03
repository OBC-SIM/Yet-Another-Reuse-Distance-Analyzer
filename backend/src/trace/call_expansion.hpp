#pragma once

#include <nlohmann/json.hpp>

#include "expansion_budget.hpp"

namespace yarda::detail
{

nlohmann::json expand_calls(const nlohmann::json & raw,
                            ExpansionBudget & budget);

nlohmann::json expand_task_calls(const nlohmann::json & raw,
                                 ExpansionBudget & budget);

}  // namespace yarda::detail
