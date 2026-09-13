#include "fixture.hpp"

#include <fstream>
#include <iterator>

#include "yarda/cache/yaml_config_parser.hpp"

namespace yarda::test::e2e
{
namespace
{
std::string read(const std::string & path)
{
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot read fixture " + path);
  return {std::istreambuf_iterator<char>(input), {}};
}
} // namespace

void GeneratedArtifacts::SetUp()
{
  raw = nlohmann::json::parse(read(input_paths[0]));
  const auto image = parse_elf_data_regions(input_paths[1]);
  ASSERT_EQ(image.image_type, ElfImageType::Executable);
  ASSERT_FALSE(image.image_relative);
  objects = build_elf_object_addresses(image);
  metadata.hierarchy = select_analysis_hierarchy(parse_cache_config(input_paths[2]));
  result_bytes = read(input_paths[3]);
  event_bytes = read(input_paths[4]);
  result = nlohmann::json::parse(result_bytes);
  events = nlohmann::json::parse(event_bytes);
  golden = nlohmann::json::parse(read(input_paths[5]));
  metadata.identity = {result.at("tool_version"),
                       sha256_file_bytes(input_paths[0]),
                       sha256_file_bytes(input_paths[1]),
                       sha256_file_bytes(input_paths[2])};
  metadata.lat_schema_version = raw.at("schema_version");
  metadata.cache_schema_version = 1;
  metadata.elf_address_size = image.address_size;
  metadata.elf_machine = image.machine;
  for (const auto & [name, base] : golden.at("bases").items())
    ASSERT_EQ(objects.objects.at(name).base, base.get<std::uint64_t>()) << name;
  for (const auto & task : golden.at("tasks"))
  {
    ResolvedTaskTrace trace;
    trace.task_id = task.at("id");
    for (const auto & row : task.at("sources"))
    {
      const auto object = row.at(0).get<std::string>();
      const auto offset = row.at(1).get<std::uint64_t>();
      const auto operation = row.at(3).get<std::string>();
      ASSERT_TRUE(operation == "load" || operation == "store");
      trace.accesses.push_back({object, offset, row.at(2),
        objects.objects.at(object).base + offset, AddressBasis::Absolute,
        operation == "load" ? AccessOperation::Load : AccessOperation::Store,
        trace.accesses.size()});
    }
    trace.coverage.source_accesses = trace.accesses.size();
    trace.coverage.resolved_accesses = trace.accesses.size();
    expected.coverage.source_accesses += trace.accesses.size();
    expected.coverage.resolved_accesses += trace.accesses.size();
    expected.tasks.push_back(std::move(trace));
  }
}
} // namespace yarda::test::e2e
