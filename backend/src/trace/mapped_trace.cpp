#include "yarda/trace/mapped_trace.hpp"

#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "block_trace.hpp"
#include "unroller.hpp"
#include "yarda/trace/resolved_mapping.hpp"

namespace yarda
{
namespace
{

void validate_mapping_geometry(const CacheGeometry & geometry)
{
  static_cast<void>(cache_set_count(geometry));
}

bool equal_coverage(const TraceCoverage & left, const TraceCoverage & right)
{
  return left.source_accesses == right.source_accesses &&
         left.resolved_accesses == right.resolved_accesses &&
         left.rejected_accesses == right.rejected_accesses &&
         left.emitted_line_references == right.emitted_line_references;
}

void validate_resolved_task_result(const ResolvedTaskTraceResult & resolved)
{
  if (resolved.tasks.empty())
  {
    throw std::invalid_argument("resolved task result contains no tasks");
  }
  std::unordered_set<std::string> task_ids;
  TraceCoverage aggregate;
  std::uint64_t excluded_opaque_call_sites = 0;
  for (const auto & task : resolved.tasks)
  {
    if (task.task_id.empty() || !task_ids.insert(task.task_id).second)
    {
      throw std::invalid_argument(
        "resolved task identity is empty or duplicate");
    }
    if (!task.coverage.complete() ||
        task.coverage.emitted_line_references != 0 ||
        task.coverage.resolved_accesses != task.accesses.size())
    {
      throw std::invalid_argument("resolved task coverage is incomplete");
    }
    aggregate.source_accesses += task.coverage.source_accesses;
    aggregate.resolved_accesses += task.coverage.resolved_accesses;
    aggregate.rejected_accesses += task.coverage.rejected_accesses;
    excluded_opaque_call_sites += task.excluded_opaque_call_sites;
  }
  if (!resolved.coverage.complete() ||
      resolved.coverage.emitted_line_references != 0 ||
      !equal_coverage(aggregate, resolved.coverage))
  {
    throw std::invalid_argument("resolved task aggregate coverage is invalid");
  }
  if (excluded_opaque_call_sites != resolved.excluded_opaque_call_sites)
  {
    throw std::invalid_argument("resolved task exclusions are inconsistent");
  }
}

std::vector<CacheLineMapping> map_resolved_accesses(
  const std::vector<ResolvedAccess> & accesses, const CacheGeometry & geometry)
{
  std::vector<CacheLineMapping> result;
  for (const auto & access : accesses)
  {
    auto mappings = map_cache_lines(access, geometry);
    result.insert(result.end(), std::make_move_iterator(mappings.begin()),
                  std::make_move_iterator(mappings.end()));
  }
  return result;
}

template <typename Trace>
void collect_mappings(const std::vector<Trace> & traces,
                      CacheLineMappingTable & mappings)
{
  for (const auto & trace : traces)
  {
    for (const auto & mapping : trace.accesses)
    {
      mappings.emplace(
        std::make_pair(mapping.object_id, mapping.object_byte_offset),
        CacheLineAddressMapping{mapping.object_id, mapping.object_byte_offset,
                                mapping.address_basis, mapping.decoded});
    }
  }
}

}  // namespace

std::vector<CacheLineMapping>
flatten_mapped_traces(const std::vector<NamedMappedTrace> & traces)
{
  std::vector<CacheLineMapping> result;
  for (const auto & trace : traces)
  {
    result.insert(result.end(), trace.accesses.begin(), trace.accesses.end());
  }
  return result;
}

MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects)
{
  validate_mapping_geometry(geometry);
  try
  {
    const detail::AccessLayoutResolver layouts(raw);
    detail::ResolvedTraceUnroller unroller(objects, layouts);
    MappedTraceResult result;
    result.traces =
      detail::build_block_traces<NamedMappedTrace, CacheLineMapping>(
        raw,
        [&unroller, &geometry](const std::string & task_id,
                               const nlohmann::json & node) {
          return map_resolved_accesses(unroller.unroll(node, task_id),
                                       geometry);
        },
        detail::EmptyLoopPolicy::Omit);
    result.coverage = unroller.coverage();
    for (const auto & trace : result.traces)
    {
      result.coverage.emitted_line_references += trace.accesses.size();
    }
    collect_mappings(result.traces, result.mappings);
    return result;
  }
  catch (const nlohmann::json::exception & error)
  {
    throw std::invalid_argument("malformed LAT input: " +
                                std::string(error.what()));
  }
}

MappedTaskTraceResult map_resolved_task_traces(
  const ResolvedTaskTraceResult & resolved, const CacheGeometry & geometry)
{
  validate_mapping_geometry(geometry);
  validate_resolved_task_result(resolved);
  MappedTaskTraceResult result;
  result.coverage = resolved.coverage;
  result.excluded_opaque_call_sites = resolved.excluded_opaque_call_sites;
  result.coverage.emitted_line_references = 0;
  for (const auto & task : resolved.tasks)
  {
    auto accesses = map_resolved_accesses(task.accesses, geometry);
    auto coverage = task.coverage;
    coverage.emitted_line_references = accesses.size();
    result.coverage.emitted_line_references += accesses.size();
    result.tasks.push_back({task.task_id, std::move(accesses),
                            std::move(coverage),
                            task.excluded_opaque_call_sites});
  }
  collect_mappings(result.tasks, result.mappings);
  return result;
}

MappedTaskTraceResult mapped_task_traces(const nlohmann::json & raw,
                                         const CacheGeometry & geometry,
                                         const ObjectAddressModel & objects)
{
  validate_mapping_geometry(geometry);
  return map_resolved_task_traces(resolved_task_traces(raw, objects), geometry);
}

}  // namespace yarda
