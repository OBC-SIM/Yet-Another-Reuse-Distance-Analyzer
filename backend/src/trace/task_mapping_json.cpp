#include "yarda/trace/task_mapping_json.hpp"

#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace yarda
{
namespace
{

constexpr std::string_view kAddressBasis = "linked_absolute";

void require_absolute(AddressBasis basis)
{
  if (basis != AddressBasis::Absolute)
  {
    throw std::invalid_argument(
      "task mapping report requires linked absolute addresses");
  }
}

std::string_view operation_name(AccessOperation operation)
{
  switch (operation)
  {
    case AccessOperation::Load:
      return "load";
    case AccessOperation::Store:
      return "store";
    case AccessOperation::Unknown:
      break;
  }
  throw std::invalid_argument("task mapping contains an unknown operation");
}

nlohmann::json coverage_json(const TraceCoverage & coverage)
{
  return {
    {"source_accesses", coverage.source_accesses},
    {"resolved_accesses", coverage.resolved_accesses},
    {"rejected_accesses", coverage.rejected_accesses},
    {"emitted_line_references", coverage.emitted_line_references},
    {"complete", coverage.complete()},
  };
}

nlohmann::json exclusions_json(std::uint64_t opaque_call_sites)
{
  return {{"known_non_inline_static_call_sites", opaque_call_sites}};
}

std::string_view elf_image_type_name(ElfImageType image_type)
{
  if (image_type == ElfImageType::Executable)
  {
    return "ET_EXEC";
  }
  throw std::invalid_argument("task mapping report requires an ET_EXEC image");
}

nlohmann::json resolved_access_json(const ResolvedAccess & access)
{
  require_absolute(access.address_basis);
  return {
    {"object_id", access.object_id},
    {"object_byte_offset", access.object_byte_offset},
    {"access_size", access.access_size},
    {"linked_byte_address", access.linked_byte_address},
    {"address_basis", kAddressBasis},
    {"operation", operation_name(access.operation)},
    {"source_access_ordinal", access.source_access_ordinal},
  };
}

nlohmann::json mapped_access_json(const CacheLineMapping & mapping)
{
  require_absolute(mapping.address_basis);
  return {
    {"object_id", mapping.object_id},
    {"object_byte_offset", mapping.object_byte_offset},
    {"linked_byte_address", mapping.decoded.address},
    {"address_basis", kAddressBasis},
    {"block_number", mapping.decoded.block_number},
    {"tag", mapping.decoded.tag},
    {"set_index", mapping.decoded.set_index},
    {"line_offset", mapping.decoded.line_offset},
    {"source_object_byte_offset", mapping.source_object_byte_offset},
    {"source_access_size", mapping.source_access_size},
    {"source_linked_byte_address", mapping.source_linked_byte_address},
    {"operation", operation_name(mapping.operation)},
    {"source_access_ordinal", mapping.source_access_ordinal},
    {"line_span_ordinal", mapping.line_span_ordinal},
  };
}

nlohmann::json mapping_table_json(const CacheLineMappingTable & mappings)
{
  auto result = nlohmann::json::array();
  for (const auto & [key, mapping] : mappings)
  {
    static_cast<void>(key);
    require_absolute(mapping.address_basis);
    result.push_back({
      {"object_id", mapping.object_id},
      {"object_byte_offset", mapping.object_byte_offset},
      {"linked_byte_address", mapping.decoded.address},
      {"address_basis", kAddressBasis},
      {"block_number", mapping.decoded.block_number},
      {"tag", mapping.decoded.tag},
      {"set_index", mapping.decoded.set_index},
      {"line_offset", mapping.decoded.line_offset},
    });
  }
  return result;
}

bool equal_coverage(const TraceCoverage & left, const TraceCoverage & right)
{
  return left.source_accesses == right.source_accesses &&
         left.resolved_accesses == right.resolved_accesses &&
         left.rejected_accesses == right.rejected_accesses &&
         left.emitted_line_references == right.emitted_line_references;
}

bool equal_resolution_counts(const TraceCoverage & left,
                             const TraceCoverage & right)
{
  return left.source_accesses == right.source_accesses &&
         left.resolved_accesses == right.resolved_accesses &&
         left.rejected_accesses == right.rejected_accesses;
}

void validate_inputs(const TaskMappingReportMetadata & metadata,
                     const ResolvedTaskTraceResult & resolved,
                     const MappedTaskTraceResult & mapped)
{
  static_cast<void>(cache_set_count(metadata.geometry));
  if (metadata.lat_path.empty() || metadata.elf_path.empty() ||
      metadata.cache_path.empty() || metadata.cache_name.empty() ||
      !metadata.elf_image_type.has_value() || metadata.elf_address_size == 0)
  {
    throw std::invalid_argument("task mapping report metadata is incomplete");
  }
  static_cast<void>(elf_image_type_name(*metadata.elf_image_type));
  if (resolved.tasks.size() != mapped.tasks.size() ||
      resolved.excluded_opaque_call_sites != mapped.excluded_opaque_call_sites)
  {
    throw std::invalid_argument("resolved and mapped task results differ");
  }
  if (!resolved.coverage.complete() || !mapped.coverage.complete() ||
      resolved.coverage.emitted_line_references != 0 ||
      !equal_resolution_counts(resolved.coverage, mapped.coverage))
  {
    throw std::invalid_argument("task mapping coverage is incomplete");
  }
  TraceCoverage resolved_aggregate;
  TraceCoverage mapped_aggregate;
  std::uint64_t excluded_aggregate = 0;
  for (std::size_t index = 0; index < resolved.tasks.size(); ++index)
  {
    const auto & resolved_task = resolved.tasks[index];
    const auto & mapped_task = mapped.tasks[index];
    if (resolved_task.task_id != mapped_task.task_id ||
        resolved_task.excluded_opaque_call_sites !=
          mapped_task.excluded_opaque_call_sites)
    {
      throw std::invalid_argument("resolved and mapped task identities differ");
    }
    if (!resolved_task.coverage.complete() ||
        !mapped_task.coverage.complete() ||
        resolved_task.coverage.emitted_line_references != 0 ||
        resolved_task.coverage.resolved_accesses !=
          resolved_task.accesses.size() ||
        mapped_task.coverage.emitted_line_references !=
          mapped_task.accesses.size() ||
        !equal_resolution_counts(resolved_task.coverage, mapped_task.coverage))
    {
      throw std::invalid_argument("task mapping task coverage is inconsistent");
    }
    resolved_aggregate.source_accesses +=
      resolved_task.coverage.source_accesses;
    resolved_aggregate.resolved_accesses +=
      resolved_task.coverage.resolved_accesses;
    resolved_aggregate.rejected_accesses +=
      resolved_task.coverage.rejected_accesses;
    mapped_aggregate.source_accesses += mapped_task.coverage.source_accesses;
    mapped_aggregate.resolved_accesses +=
      mapped_task.coverage.resolved_accesses;
    mapped_aggregate.rejected_accesses +=
      mapped_task.coverage.rejected_accesses;
    mapped_aggregate.emitted_line_references +=
      mapped_task.coverage.emitted_line_references;
    excluded_aggregate += resolved_task.excluded_opaque_call_sites;
  }
  if (!equal_coverage(resolved_aggregate, resolved.coverage) ||
      !equal_coverage(mapped_aggregate, mapped.coverage) ||
      excluded_aggregate != resolved.excluded_opaque_call_sites)
  {
    throw std::invalid_argument("task mapping aggregate metadata is invalid");
  }
}

}  // namespace

nlohmann::json task_mapping_json(const TaskMappingReportMetadata & metadata,
                                 const ResolvedTaskTraceResult & resolved,
                                 const MappedTaskTraceResult & mapped)
{
  validate_inputs(metadata, resolved, mapped);
  auto tasks = nlohmann::json::array();
  for (std::size_t index = 0; index < resolved.tasks.size(); ++index)
  {
    const auto & resolved_task = resolved.tasks[index];
    const auto & mapped_task = mapped.tasks[index];
    auto resolved_accesses = nlohmann::json::array();
    for (const auto & access : resolved_task.accesses)
    {
      resolved_accesses.push_back(resolved_access_json(access));
    }
    auto mapped_accesses = nlohmann::json::array();
    for (const auto & access : mapped_task.accesses)
    {
      mapped_accesses.push_back(mapped_access_json(access));
    }
    tasks.push_back({
      {"task_id", resolved_task.task_id},
      {"coverage", coverage_json(mapped_task.coverage)},
      {"exclusions", exclusions_json(mapped_task.excluded_opaque_call_sites)},
      {"resolved_accesses", std::move(resolved_accesses)},
      {"mapped_line_references", std::move(mapped_accesses)},
    });
  }

  return {
    {"schema_version", 1},
    {"mode", "elf-task-mapping"},
    {"inputs",
     {{"lat", metadata.lat_path},
      {"elf", metadata.elf_path},
      {"cache", metadata.cache_path}}},
    {"elf",
     {{"type", elf_image_type_name(*metadata.elf_image_type)},
      {"address_size", metadata.elf_address_size},
      {"machine", metadata.elf_machine},
      {"address_basis", kAddressBasis}}},
    {"cache",
     {{"name", metadata.cache_name},
      {"line_size", metadata.geometry.line_size},
      {"line_count", metadata.geometry.line_count},
      {"associativity", metadata.geometry.associativity},
      {"set_count", cache_set_count(metadata.geometry)}}},
    {"coverage", coverage_json(mapped.coverage)},
    {"exclusions", exclusions_json(mapped.excluded_opaque_call_sites)},
    {"tasks", std::move(tasks)},
    {"mappings", mapping_table_json(mapped.mappings)},
  };
}

}  // namespace yarda
