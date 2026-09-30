#pragma once

#include "yarda/cache/hierarchy_result_json.hpp"

namespace yarda::detail
{

/** @brief Serialize a validated cache snapshot in schema field order. */
nlohmann::ordered_json hierarchy_level_json(const AnalysisCacheLevel & level);

/** @brief Serialize a validated task without recalculating its ratios. */
nlohmann::ordered_json hierarchy_task_json(const TaskHierarchySummary & task);

} // namespace yarda::detail
