#pragma once

#include "yarda/trace/resolved_access.hpp"

namespace yarda::rtems_experiment
{
/**
 * @brief Construct independent sources from kernel equations and GNU nm bases.
 * @param row Prepared case; contains dimensions and ELF symbol bases/sizes.
 * @return One cold task, without reading LAT or production mapping output.
 */
ResolvedTaskTraceResult expected_sources(const nlohmann::json & row);
}
