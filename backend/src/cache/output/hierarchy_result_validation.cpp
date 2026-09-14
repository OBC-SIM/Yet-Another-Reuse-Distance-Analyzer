#include "hierarchy_result_validation.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

#include "cache/hierarchy/hierarchy_service_summary.hpp"
#include "cache/streaming/streaming_hierarchy_summary.hpp"

namespace yarda::detail
{
namespace
{

void validate_level(const CacheLevelSummary & level,
                    const CacheGeometry & geometry)
{
  std::uint64_t hits = 0;
  std::uint64_t replacements = 0;
  for (const auto & [distance, count] : level.csrd_histogram)
  {
    if (count == 0)
      throw std::invalid_argument("hierarchy histogram contains a zero count");
    if (distance >= level.unique_lines)
      throw std::invalid_argument(
          "hierarchy distance exceeds distinct-line history");
    auto & total = distance < geometry.associativity ? hits : replacements;
    total = checked_service_sum(total, count);
  }
  static_cast<void>(checked_service_sum(hits, replacements));
  if (hits != level.hits || replacements != level.replacement_misses ||
      level.unique_lines != level.cold_misses)
    throw std::invalid_argument("hierarchy histogram or unique lines disagree");
}

void validate_task(const TaskHierarchySummary & task,
                   const AnalysisHierarchy & hierarchy)
{
  const auto & flags = task.invariants;
  if (!flags.level_conservation_l1 || !flags.level_conservation_llc ||
      !flags.llc_input_matches_l1_misses || !flags.first_service_conservation ||
      !flags.all_passed)
    throw std::invalid_argument("hierarchy result has a failed invariant");
  validate_level(task.l1, hierarchy.l1.geometry);
  validate_level(task.llc, hierarchy.llc.geometry);
  if (task.llc.unique_lines > task.l1.unique_lines)
    throw std::invalid_argument(
        "hierarchy LLC has more distinct lines than L1");
  if (task.source_accesses > task.modeled_accesses ||
      (task.source_accesses == 0 && task.modeled_accesses != 0))
    throw std::invalid_argument("hierarchy source and line counts disagree");
  const auto checked = finalize_hierarchy_service(task);
  for (const auto & ratio : {task.l1_first_hit_ratio, task.llc_first_hit_ratio,
                            task.all_cache_miss_ratio})
    if (ratio && std::signbit(*ratio))
      throw std::invalid_argument("hierarchy ratio has a negative encoding");
  if (task.l1_first_hit_ratio != checked.l1_first_hit_ratio ||
      task.llc_first_hit_ratio != checked.llc_first_hit_ratio ||
      task.all_cache_miss_ratio != checked.all_cache_miss_ratio)
    throw std::invalid_argument(
        "hierarchy ratios disagree with service counts");
}

} // namespace

std::uint64_t hierarchy_level_capacity(const AnalysisCacheLevel & level)
{
  static_cast<void>(cache_set_count(level.geometry));
  if (level.geometry.line_count >
      std::numeric_limits<std::uint64_t>::max() / level.geometry.line_size)
    throw std::overflow_error("hierarchy capacity overflows uint64_t bytes");
  return level.geometry.line_count * level.geometry.line_size;
}

void validate_hierarchy_result(const HierarchyResultMetadata & metadata,
                               const StreamingHierarchyResult & result)
{
  const auto & hierarchy = metadata.hierarchy;
  if (metadata.lat_schema_version != 2 || metadata.cache_schema_version != 1 ||
      (metadata.elf_address_size != 4 && metadata.elf_address_size != 8) ||
      metadata.elf_machine == 0)
    throw std::invalid_argument(
        "hierarchy result input metadata is incomplete");
  if (hierarchy.core_id != 0 ||
      hierarchy.core_id != metadata.identity.analysis_core_id ||
      hierarchy.l1.name.empty() || hierarchy.llc.name.empty() ||
      hierarchy.memory_name.empty() || hierarchy.l1.role != "L1" ||
      hierarchy.llc.role != "LLC" || hierarchy.l1.name == hierarchy.llc.name ||
      hierarchy.l1.name == hierarchy.memory_name ||
      hierarchy.llc.name == hierarchy.memory_name ||
      hierarchy.l1.geometry.line_size != hierarchy.llc.geometry.line_size)
    throw std::invalid_argument("hierarchy result selected path is invalid");
  static_cast<void>(hierarchy_level_capacity(hierarchy.l1));
  static_cast<void>(hierarchy_level_capacity(hierarchy.llc));
  if (result.tasks.empty())
    throw std::invalid_argument("hierarchy result requires an analyzed task");
  std::unordered_set<std::string> identities;
  TraceCoverage aggregate;
  for (const auto & task : result.tasks)
  {
    if (task.task_id.empty() || !identities.insert(task.task_id).second)
      throw std::invalid_argument(
          "hierarchy task identity is empty or repeated");
    validate_task(task, hierarchy);
    add_streaming_coverage(aggregate, task.coverage);
  }
  if (!result.coverage.complete() ||
      aggregate.source_accesses != result.coverage.source_accesses ||
      aggregate.resolved_accesses != result.coverage.resolved_accesses ||
      aggregate.rejected_accesses != result.coverage.rejected_accesses ||
      aggregate.emitted_line_references !=
          result.coverage.emitted_line_references)
    throw std::invalid_argument(
        "hierarchy module coverage disagrees with tasks");
}

} // namespace yarda::detail
