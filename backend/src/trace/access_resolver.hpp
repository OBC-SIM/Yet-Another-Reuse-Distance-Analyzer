#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "access_layout.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/trace_coverage.hpp"

namespace yarda::detail
{

ResolvedAccess resolve_access(const nlohmann::json & node,
                              const std::vector<std::string> & indices,
                              const std::string & task_id,
                              std::uint64_t source_access_ordinal,
                              const ObjectAddressModel & objects,
                              const AccessLayoutResolver & layouts,
                              TraceCoverage & coverage);

}  // namespace yarda::detail
