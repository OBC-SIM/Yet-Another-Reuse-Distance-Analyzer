#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "yarda/reuse/profile.hpp"
#include "yarda/trace/trace.hpp"

namespace
{

using Json = nlohmann::json;

struct Options
{
  std::string input;
  yarda::Granularity granularity = yarda::Granularity::Element;
  std::size_t cache_line_size = 32;
  std::string export_path;
};

void print_usage()
{
  std::cout << "Usage: yarda_cpp LAT.json [--mode unroll]"
            << " [--granularity element|cache-line]"
            << " [--cache-line-size N] [--export PATH]\n";
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
    else if (argument == "--cache-line-size")
    {
      options.cache_line_size = std::stoull(next(argument));
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
  if (options.cache_line_size == 0)
  {
    throw std::invalid_argument("cache-line size must be positive");
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

    yarda::ReuseProfile program;
    Json block_payload = Json::array();
    const auto blocks =
      yarda::block_traces(raw, options.granularity, options.cache_line_size);
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
                              ? Json(options.cache_line_size)
                              : Json(nullptr)},
        {"program", profile_json(program)},
        {"blocks", block_payload},
      };
      std::ofstream output(options.export_path);
      if (!output)
      {
        throw std::runtime_error("cannot open export path: " +
                                 options.export_path);
      }
      output << payload.dump(2) << '\n';
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
