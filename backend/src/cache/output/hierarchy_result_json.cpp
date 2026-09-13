#include "yarda/cache/hierarchy_result_json.hpp"

#include "artifact_contract.hpp"
#include "hierarchy_json_fields.hpp"
#include "hierarchy_result_validation.hpp"

namespace yarda
{

nlohmann::ordered_json
hierarchy_result_json(const HierarchyResultMetadata & metadata,
                      const StreamingHierarchyResult & result)
{
  detail::validate_hierarchy_result(metadata, result);
  const auto id = hierarchy_analysis_id(metadata.identity);
  const auto & hierarchy = metadata.hierarchy;
  auto tasks = nlohmann::ordered_json::array();
  for (const auto & task : result.tasks)
    tasks.push_back(detail::hierarchy_task_json(task));
  return {
      {"schema_version", detail::kArtifactSchemaVersion},
      {"analysis_mode", detail::kAnalysisMode},
      {"model_id", detail::kModelId},
      {"csrd_mode", detail::kCsrdMode},
      {"address_basis", detail::kAddressBasis},
      {"tool_version", metadata.identity.tool_version},
      {"analysis_id", id},
      {"inputs",
       {{"lat_sha256", metadata.identity.lat_sha256},
        {"elf_sha256", metadata.identity.elf_sha256},
        {"cache_config_sha256", metadata.identity.cache_config_sha256},
        {"lat_schema_version", metadata.lat_schema_version},
        {"cache_schema_version", metadata.cache_schema_version},
        {"elf_class", metadata.elf_address_size == 4 ? "ELF32" : "ELF64"},
        {"elf_machine", metadata.elf_machine}}},
      {"selected_path",
       {{"core_id", hierarchy.core_id},
        {"l1_name", hierarchy.l1.name},
        {"llc_name", hierarchy.llc.name},
        {"memory_name", hierarchy.memory_name}}},
      {"cache_hierarchy",
       {{"levels",
         {detail::hierarchy_level_json(hierarchy.l1),
          detail::hierarchy_level_json(hierarchy.llc)}},
        {"allocation", "all-demand-misses"},
        {"lower_level_requests", "l1-misses-only"},
        {"inclusion", "independent-no-back-invalidation-or-victim-insertion"},
        {"task_initial_state", "cold"}}},
      {"tasks", std::move(tasks)},
  };
}

} // namespace yarda
