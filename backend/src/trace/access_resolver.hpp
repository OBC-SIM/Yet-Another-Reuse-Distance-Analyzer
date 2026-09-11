#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "access_layout.hpp"
#include "prepared_access.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/trace_coverage.hpp"

namespace yarda::detail
{

/**
 * @brief Resolve current indices and retain only a successful immutable plan.
 * @param node Borrowed immutable node after inline binding.
 * @param prepared Traversal-owned reusable indices and optional successful
 * plan.
 * @param exact_indices Whether the current numeric row is entirely exact.
 * @param task_id Borrowed diagnostic task identity.
 * @param source_access_ordinal Already charged source position.
 * @param objects Borrowed immutable ELF model for this traversal.
 * @param layouts Borrowed immutable ABI metadata for this traversal.
 * @param coverage Module coverage, with this source already counted.
 * @return Value-owned current access after all dynamic range checks.
 * @throws ResolutionError with original category, provenance and coverage.
 */
ResolvedAccess resolve_access(const nlohmann::json & node,
                              PreparedAccess & prepared, bool exact_indices,
                              const std::string & task_id,
                              std::uint64_t source_access_ordinal,
                              const ObjectAddressModel & objects,
                              const AccessLayoutResolver & layouts,
                              TraceCoverage & coverage);

}  // namespace yarda::detail
