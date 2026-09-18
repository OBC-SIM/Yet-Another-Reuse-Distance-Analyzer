#include "evaluation.hpp"

#include <fstream>

#include "build_version.hpp"
#include "yarda/cache/yaml_config_parser.hpp"
#include "yarda/elf/data_regions.hpp"
#include "yarda/elf/object_addresses.hpp"

namespace yarda::evaluation
{

nlohmann::json read_json(const std::string & path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("cannot read JSON: " + path);
  return nlohmann::json::parse(file);
}

StreamingHierarchyOptions work_options(const nlohmann::json & row)
{
  const auto & limits = row.at("effective_work_limits");
  const auto number = [&](const char * key) {
    const auto & value = limits.at(key);
    if (!value.is_number_unsigned() &&
        !(value.is_number_integer() && value.get<std::int64_t>() >= 0))
      throw std::invalid_argument(std::string("invalid work limit: ") + key);
    return value.get<std::uint64_t>();
  };
  StreamingHierarchyOptions result;
  result.loop_limits = {number("single_loop"), number("cumulative_loop")};
  result.emission_limits = {number("source_accesses"), number("line_references")};
  return result;
}

Inputs read_inputs(const nlohmann::json & row)
{
  Inputs result;
  const auto map = row.at("map_path").get<std::string>();
  const auto elf = row.at("elf_path").get<std::string>();
  const auto cache = row.at("cache_path").get<std::string>();
  auto & metadata = result.metadata;
  metadata.identity.tool_version = cli::kToolVersion;
  metadata.identity.map_sha256 = sha256_file_bytes(map);
  metadata.identity.elf_sha256 = sha256_file_bytes(elf);
  metadata.identity.cache_config_sha256 = sha256_file_bytes(cache);
  result.raw = read_json(map);
  if (!result.raw.is_object() || !result.raw.contains("schema_version") ||
      !result.raw["schema_version"].is_number_integer() ||
      result.raw["schema_version"] != 2)
    throw std::invalid_argument("hierarchy-rd requires MAP schema_version 2");
  metadata.map_schema_version = 2;
  const auto config = parse_cache_config(cache);
  metadata.cache_schema_version = config.schema_version;
  metadata.hierarchy = select_analysis_hierarchy(config);
  const auto image = parse_elf_data_regions(elf);
  if (image.image_type != ElfImageType::Executable || image.image_relative)
    throw std::invalid_argument("hierarchy-rd requires an ET_EXEC image");
  result.objects = build_elf_object_addresses(image);
  if (result.objects.basis != AddressBasis::Absolute)
    throw std::invalid_argument("hierarchy-rd requires absolute linked addresses");
  metadata.elf_address_size = image.address_size;
  metadata.elf_machine = image.machine;
  return result;
}

} // namespace yarda::evaluation
