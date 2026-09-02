#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "yarda/cache/cache_config.hpp"
#include "yarda/cache/yaml_config_parser.hpp"
#include "yarda/elf/data_regions.hpp"
#include "yarda/elf/object_addresses.hpp"
#include "yarda/reuse/profile.hpp"
#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/task_mapping_json.hpp"
#include "yarda/trace/trace.hpp"

namespace
{

using Json = nlohmann::json;

struct Options
{
  std::string input;
  yarda::Granularity granularity = yarda::Granularity::Element;
  bool granularity_explicit = false;
  std::string cache_path;
  std::string elf_path;
  std::string export_path;
};

void print_usage()
{
  std::cout << "Usage: yarda_cpp LAT.json [--mode unroll]"
            << " [--granularity element|cache-line]"
            << " [--cache FILE] [--elf FILE] [--export PATH]\n";
}

Options parse_options(int argc, char ** argv)
{
  Options options;
  for (int index = 1; index < argc; ++index)
  {
    const std::string argument = argv[index];
    const auto next = [&](const std::string & name) -> std::string {
      if (index + 1 >= argc)
      {
        throw std::invalid_argument(name + " requires a value");
      }
      return argv[++index];
    };
    if (argument == "--mode")
    {
      const auto mode = next(argument);
      if (mode != "unroll")
      {
        throw std::invalid_argument("unknown mode: " + mode);
      }
    }
    else if (argument == "--granularity")
    {
      options.granularity_explicit = true;
      const auto value = next(argument);
      if (value == "element")
      {
        options.granularity = yarda::Granularity::Element;
      }
      else if (value == "cache-line")
      {
        options.granularity = yarda::Granularity::CacheLine;
      }
      else
      {
        throw std::invalid_argument("unknown granularity: " + value);
      }
    }
    else if (argument == "--cache")
    {
      options.cache_path = next(argument);
    }
    else if (argument == "--elf")
    {
      options.elf_path = next(argument);
    }
    else if (argument == "--export")
    {
      options.export_path = next(argument);
    }
    else if (argument == "--help" || argument == "-h")
    {
      print_usage();
      std::exit(0);
    }
    else if (!argument.empty() && argument.front() == '-')
    {
      throw std::invalid_argument("unknown option: " + argument);
    }
    else if (options.input.empty())
    {
      options.input = argument;
    }
    else
    {
      throw std::invalid_argument("only one LAT input is supported");
    }
  }
  if (options.input.empty())
  {
    throw std::invalid_argument("LAT input path is required");
  }
  if (!options.elf_path.empty() && options.cache_path.empty())
  {
    throw std::invalid_argument("--cache is required with --elf");
  }
  if (!options.elf_path.empty() && options.granularity_explicit &&
      options.granularity != yarda::Granularity::CacheLine)
  {
    throw std::invalid_argument(
      "--granularity element is incompatible with --elf");
  }
  if (options.granularity == yarda::Granularity::CacheLine &&
      options.cache_path.empty())
  {
    throw std::invalid_argument(
      "--cache is required for cache-line granularity");
  }
  return options;
}

Json profile_json(const yarda::ReuseProfile & profile)
{
  Json histogram = Json::object();
  std::uint64_t total = 0;
  for (const auto & [distance, count] : profile.histogram)
  {
    histogram[std::to_string(distance)] = count;
    total += count;
  }
  return {
    {"histogram", histogram},
    {"cold_misses", profile.cold_misses.size()},
    {"total_reuses", total},
  };
}

void print_profile(const yarda::ReuseProfile & profile)
{
  std::uint64_t total = 0;
  std::cout << "      RD       count\n";
  std::cout << "  ------  ----------\n";
  for (const auto & [distance, count] : profile.histogram)
  {
    std::cout << "  " << distance << "\t" << count << '\n';
    total += count;
  }
  std::cout << "  total\t" << total << '\n';
  std::cout << "  cold misses: " << profile.cold_misses.size() << '\n';
}

void write_json(const Json & payload, const std::string & path)
{
  const auto document = payload.dump(2);
  if (path.empty())
  {
    std::cout << document << '\n';
    std::cout.flush();
    if (!std::cout)
    {
      throw std::runtime_error("failed to write JSON to stdout");
    }
    return;
  }
  std::ofstream output(path);
  if (!output)
  {
    throw std::runtime_error("cannot open export path: " + path);
  }
  output << document << '\n';
  output.close();
  if (!output)
  {
    throw std::runtime_error("failed to write export path: " + path);
  }
}

void map_elf_tasks(const Options & options, const Json & raw,
                   const yarda::HierarchyConfig & config)
{
  const auto image = yarda::parse_elf_data_regions(options.elf_path);
  if (image.image_type != yarda::ElfImageType::Executable ||
      image.image_relative)
  {
    throw std::invalid_argument("--elf task mapping requires an ET_EXEC image");
  }
  const auto objects = yarda::build_elf_object_addresses(image);
  if (objects.basis != yarda::AddressBasis::Absolute)
  {
    throw std::invalid_argument(
      "--elf task mapping requires absolute linked addresses");
  }
  const auto & cache = yarda::entry_cache_config(config, 0);
  const auto geometry = yarda::make_cache_geometry(cache);
  const auto resolved = yarda::resolved_task_traces(raw, objects);
  const auto mapped = yarda::map_resolved_task_traces(resolved, geometry);
  yarda::TaskMappingReportMetadata metadata;
  metadata.lat_path = options.input;
  metadata.elf_path = options.elf_path;
  metadata.cache_path = options.cache_path;
  metadata.elf_image_type = image.image_type;
  metadata.elf_address_size = image.address_size;
  metadata.elf_machine = image.machine;
  metadata.cache_name = cache.name;
  metadata.geometry = geometry;
  write_json(yarda::task_mapping_json(metadata, resolved, mapped),
             options.export_path);
}

}  // namespace

int main(int argc, char ** argv)
{
  try
  {
    const auto options = parse_options(argc, argv);
    std::ifstream input(options.input);
    if (!input)
    {
      throw std::runtime_error("cannot open LAT input: " + options.input);
    }
    Json raw;
    input >> raw;

    std::size_t cache_line_size = 0;
    yarda::HierarchyConfig cache_config;
    if (!options.cache_path.empty())
    {
      cache_config = yarda::parse_cache_config(options.cache_path);
      const auto & l1 = yarda::entry_cache_config(cache_config, 0);
      cache_line_size = yarda::make_cache_geometry(l1).line_size;
    }

    if (!options.elf_path.empty())
    {
      map_elf_tasks(options, raw, cache_config);
      return 0;
    }

    yarda::ReuseProfile program;
    Json block_payload = Json::array();
    const auto blocks =
      yarda::block_traces(raw, options.granularity, cache_line_size);
    std::vector<std::string> program_trace;
    for (const auto & block : blocks)
    {
      const auto profile = yarda::calculate_reuse_profile(block.accesses);
      program_trace.insert(program_trace.end(), block.accesses.begin(),
                           block.accesses.end());
      block_payload.push_back({
        {"name", block.name},
        {"profile", profile_json(profile)},
      });
    }
    program = yarda::calculate_reuse_profile(program_trace);
    print_profile(program);

    if (!options.export_path.empty())
    {
      const Json payload = {
        {"file", options.input},
        {"mode", "unroll"},
        {"granularity", options.granularity == yarda::Granularity::CacheLine
                          ? "cache-line"
                          : "element"},
        {"cache_line_size", options.granularity == yarda::Granularity::CacheLine
                              ? Json(cache_line_size)
                              : Json(nullptr)},
        {"program", profile_json(program)},
        {"blocks", block_payload},
      };
      write_json(payload, options.export_path);
    }
    return 0;
  }
  catch (const std::exception & error)
  {
    std::cerr << "error: " << error.what() << '\n';
    print_usage();
    return 1;
  }
}
