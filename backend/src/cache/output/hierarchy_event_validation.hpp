#pragma once

#include "yarda/cache/hierarchy_result_json.hpp"

namespace yarda::detail
{

/** @brief Require a complete, ordered prefix with aligned cache observations.
 */
void validate_hierarchy_events(
    const HierarchyEventMetadata & metadata,
    const std::vector<HierarchyEventRecord> & events);

} // namespace yarda::detail
