#pragma once

#include "yarda/cache/hierarchy_model.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace yarda::detail
{

/**
 * @brief Enforce hierarchy-only conditions before general task remapping.
 *
 * Coverage, task identities and byte ranges remain the mapper's responsibility.
 * Checking exclusions on every task avoids accepting a wrapped module total.
 *
 * @param resolved Borrowed geometry-independent task input.
 * @param hierarchy Selected model snapshot.
 * @return Nothing.
 * @throws std::invalid_argument for invalid or unequal-line-size geometries,
 * opaque-call exclusions, non-absolute addresses, unsupported operations or
 * nonconsecutive task-local source ordinals.
 */
void validate_hierarchy_input(const ResolvedTaskTraceResult & resolved,
                              const AnalysisHierarchy & hierarchy);

}  // namespace yarda::detail
