#pragma once

#include "yarda/cache/hierarchy_result_json.hpp"

namespace yarda::detail
{

/** @brief Validate the summary boundary without reconstructing references. */
void validate_hierarchy_result(const HierarchyResultMetadata & metadata,
                               const StreamingHierarchyResult & result);

/** @brief Compute effective capacity with checked uint64 arithmetic. */
std::uint64_t hierarchy_level_capacity(const AnalysisCacheLevel & level);

} // namespace yarda::detail
