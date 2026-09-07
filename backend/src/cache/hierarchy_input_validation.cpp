#include "hierarchy_input_validation.hpp"

#include <cstddef>
#include <stdexcept>

namespace yarda::detail
{

void validate_hierarchy_input(const ResolvedTaskTraceResult & resolved,
                              const AnalysisHierarchy & hierarchy)
{
  static_cast<void>(cache_set_count(hierarchy.l1.geometry));
  static_cast<void>(cache_set_count(hierarchy.llc.geometry));
  if (hierarchy.l1.geometry.line_size != hierarchy.llc.geometry.line_size)
  {
    throw std::invalid_argument(
      "hierarchy analysis requires matching line sizes");
  }
  if (resolved.excluded_opaque_call_sites != 0)
  {
    throw std::invalid_argument(
      "hierarchy analysis rejects opaque-call exclusions");
  }
  for (const auto & task : resolved.tasks)
  {
    if (task.excluded_opaque_call_sites != 0)
    {
      throw std::invalid_argument(
        "hierarchy task has opaque-call exclusions: " + task.task_id);
    }
    for (std::size_t index = 0; index < task.accesses.size(); ++index)
    {
      const auto & access = task.accesses[index];
      if (access.address_basis != AddressBasis::Absolute)
      {
        throw std::invalid_argument(
          "hierarchy task requires absolute addresses: " + task.task_id);
      }
      if (access.operation != AccessOperation::Load &&
          access.operation != AccessOperation::Store)
      {
        throw std::invalid_argument(
          "hierarchy task requires load/store operations: " + task.task_id);
      }
      if (access.source_access_ordinal != index)
      {
        throw std::invalid_argument(
          "hierarchy task requires consecutive source ordinals: " +
          task.task_id);
      }
    }
  }
}

}  // namespace yarda::detail
