#include "hierarchy_aggregation.hpp"

#include <stdexcept>
#include <tuple>
#include <utility>

#include "hierarchy_service_summary.hpp"

namespace yarda::detail
{
namespace
{

bool same_reference(const CacheLineMapping & left,
                    const CacheLineMapping & right)
{
  return std::tie(left.source_access_ordinal, left.line_span_ordinal,
                  left.object_id, left.object_byte_offset, left.address_basis,
                  left.decoded.address, left.source_object_byte_offset,
                  left.source_access_size, left.source_linked_byte_address,
                  left.operation) ==
         std::tie(right.source_access_ordinal, right.line_span_ordinal,
                  right.object_id, right.object_byte_offset,
                  right.address_basis, right.decoded.address,
                  right.source_object_byte_offset, right.source_access_size,
                  right.source_linked_byte_address, right.operation);
}

}  // namespace

BatchTaskHierarchyResult
aggregate_hierarchy_task(std::string task_id, const TraceCoverage & coverage,
                         BatchCacheLevelResult l1, BatchCacheLevelResult llc)
{
  if (l1.accesses.size() != l1.mappings.size() ||
      l1.mappings.size() != l1.summary.lookups)
    throw std::logic_error("hierarchy L1 result size disagrees with mappings");
  if (llc.accesses.size() != llc.mappings.size() ||
      llc.mappings.size() != llc.summary.lookups)
    throw std::logic_error("hierarchy LLC result size disagrees with mappings");

  BatchTaskHierarchyResult result;
  result.events.reserve(l1.accesses.size());
  auto & summary = result.summary;
  std::size_t llc_index = 0;
  for (std::size_t index = 0; index < l1.accesses.size(); ++index)
  {
    HierarchyAccessEvent event;
    event.l1 = l1.accesses[index];
    if (event.l1.outcome == LruAccessOutcome::Hit)
    {
      event.first_service = FirstServiceLevel::L1;
      summary.ehc_l1 = checked_service_sum(summary.ehc_l1, 1);
    }
    else
    {
      if (llc_index == llc.accesses.size())
        throw std::logic_error("hierarchy missing LLC row for L1 miss");
      if (!same_reference(l1.mappings[index], llc.mappings[llc_index]))
        throw std::logic_error("hierarchy paired row provenance disagrees");
      event.llc_mapping = std::move(llc.mappings[llc_index]);
      event.llc = llc.accesses[llc_index++];
      if (event.llc->outcome == LruAccessOutcome::Hit)
      {
        event.first_service = FirstServiceLevel::LLC;
        summary.ehc_llc = checked_service_sum(summary.ehc_llc, 1);
      }
      else
      {
        event.first_service = FirstServiceLevel::Memory;
        summary.all_cache_misses =
          checked_service_sum(summary.all_cache_misses, 1);
      }
    }
    event.l1_mapping = std::move(l1.mappings[index]);
    result.events.push_back(std::move(event));
  }
  if (llc_index != llc.accesses.size())
    throw std::logic_error("hierarchy leftover LLC rows after L1 traversal");

  summary.task_id = std::move(task_id);
  summary.source_accesses = coverage.source_accesses;
  summary.modeled_accesses = l1.summary.lookups;
  summary.l1 = std::move(l1.summary);
  summary.llc = std::move(llc.summary);
  summary.coverage = coverage;
  result.summary = finalize_hierarchy_service(std::move(summary));
  return result;
}

}  // namespace yarda::detail
