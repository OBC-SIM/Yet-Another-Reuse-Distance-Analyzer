#include "hierarchy_command.hpp"

#include <fstream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "artifact_output.hpp"
#include "build_version.hpp"
#include "yarda/cache/hierarchy_result_json.hpp"
#include "yarda/cache/yaml_config_parser.hpp"
#include "yarda/elf/data_regions.hpp"
#include "yarda/elf/object_addresses.hpp"

namespace yarda::cli
{

void run_hierarchy_command(const Options & options,
                           const AnalysisTelemetryProviders * providers,
                           const ArtifactRename & rename)
{
  std::optional<AnalysisTelemetryCollector> collector;
  if (!options.telemetry_path.empty())
  {
    if (providers)
      collector.emplace(*providers);
    else
      collector.emplace();
  }
  HierarchyResultMetadata metadata;
  metadata.identity.tool_version = kToolVersion;
  metadata.identity.lat_sha256 = sha256_file_bytes(options.input);
  metadata.identity.cache_config_sha256 = sha256_file_bytes(options.cache_path);
  metadata.identity.elf_sha256 = sha256_file_bytes(options.elf_path);

  auto started = collector ? collector->now_ns() : 0;
  std::ifstream input(options.input, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot open LAT input: " + options.input);
  const auto raw = nlohmann::json::parse(input);
  if (!raw.is_object() || !raw.contains("schema_version") ||
      !raw["schema_version"].is_number_integer() || raw["schema_version"] != 2)
    throw std::invalid_argument("hierarchy-rd requires LAT schema_version 2");
  metadata.lat_schema_version = raw["schema_version"].get<std::uint32_t>();
  if (collector) collector->finish_stage(AnalysisStage::ParseLat, started);

  started = collector ? collector->now_ns() : 0;
  const auto config = parse_cache_config(options.cache_path);
  metadata.cache_schema_version = config.schema_version;
  if (collector) collector->finish_stage(AnalysisStage::ParseCache, started);

  started = collector ? collector->now_ns() : 0;
  const auto image = parse_elf_data_regions(options.elf_path);
  if (image.image_type != ElfImageType::Executable || image.image_relative)
    throw std::invalid_argument("hierarchy-rd requires an ET_EXEC image");
  const auto objects = build_elf_object_addresses(image);
  if (objects.basis != AddressBasis::Absolute)
    throw std::invalid_argument(
        "hierarchy-rd requires absolute linked addresses");
  metadata.elf_address_size = image.address_size;
  metadata.elf_machine = image.machine;
  if (collector) collector->finish_stage(AnalysisStage::ParseElf, started);
  metadata.hierarchy = select_analysis_hierarchy(config);

  StreamingHierarchyOptions analysis_options;
  analysis_options.loop_limits = options.loop_limits;
  analysis_options.emission_limits = options.emission_limits;
  analysis_options.telemetry = collector ? &*collector : nullptr;
  std::vector<HierarchyEventRecord> events;
  if (!options.events_path.empty())
  {
    analysis_options.event_limit = options.event_limit;
    analysis_options.event_sink = [&](const auto & task, const auto & event) {
      events.push_back({task, event});
    };
  }
  const auto result = analyze_streaming_hierarchy(
      raw, objects, metadata.hierarchy, analysis_options);
  started = collector ? collector->now_ns() : 0;
  const auto document = hierarchy_result_json(metadata, result);
  auto result_text = document.dump(2);
  const auto analysis_id = document.at("analysis_id").get<std::string>();
  if (collector)
    collector->finish_stage(AnalysisStage::SerializeResult, started);
  const auto telemetry =
      collector
          ? std::optional<AnalysisTelemetry>(collector->snapshot(
                analysis_id, result.coverage, result.execution_statistics))
          : std::nullopt;

  std::vector<JsonArtifact> artifacts;
  // GCC 11 can leak earlier aggregate fields when a later dump() throws.
  // Finish serialization before constructing the artifact temporary.
  if (!options.events_path.empty())
  {
    const HierarchyEventMetadata event_metadata{
        analysis_id, options.event_limit,
        result.coverage.emitted_line_references, result.event_delivery};
    auto text = hierarchy_events_json(event_metadata, events).dump(2);
    artifacts.push_back({options.events_path, std::move(text)});
  }
  if (telemetry)
  {
    auto text = hierarchy_telemetry_json(*telemetry).dump(2);
    artifacts.push_back({options.telemetry_path, std::move(text)});
  }
  // Publish RESULT last, after both optional artifacts have been staged.
  artifacts.push_back({options.export_path, std::move(result_text)});
  publish_json_artifacts({options.input, options.cache_path, options.elf_path},
                         artifacts, rename);
}

} // namespace yarda::cli
