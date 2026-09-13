#include "options.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace yarda::cli
{
namespace
{

std::uint64_t unsigned_value(const std::string & text, const std::string & flag)
{
  std::uint64_t value = 0;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || parsed.ec != std::errc{} ||
      parsed.ptr != text.data() + text.size())
    throw std::invalid_argument(flag + " requires an unsigned uint64 decimal");
  return value;
}

std::string artifact_path(std::string path, const std::string & flag)
{
  if (path.empty() || path == "-" || path.find('\0') != std::string::npos)
    throw std::invalid_argument(flag + " requires a nonempty file path");
  return path;
}

} // namespace

void print_usage()
{
  std::cout << "Usage: yarda_cpp LAT.json [--mode unroll]"
            << " [--granularity element|cache-line]"
            << " [--cache FILE] [--elf FILE] [--export PATH]\n"
            << "  --analysis mapping|hierarchy-rd (omitted: legacy dispatch)\n"
            << "  hierarchy-rd requires --elf, --cache and --export FILE\n"
            << "  --export-events FILE [--event-limit N] (default: 0)\n"
            << "  --telemetry FILE\n"
            << "  --max-single-loop-iterations N (default: 1000000)\n"
            << "  --max-cumulative-loop-iterations N (default: 1000000)\n"
            << "  --max-source-accesses N (default: 1000000)\n"
            << "  --max-line-references N (default: 10000000)\n"
            << "  Limits are inclusive; zero permits no work.\n";
}

Options parse_options(int argc, char ** argv)
{
  Options options;
  bool hierarchy_options = false;
  bool event_limit_explicit = false;
  for (int index = 1; index < argc; ++index)
  {
    const std::string argument = argv[index];
    const auto next = [&](const std::string & name) -> std::string
    {
      if (index + 1 >= argc)
      {
        throw std::invalid_argument(name + " requires a value");
      }
      return argv[++index];
    };
    if (argument == "--analysis")
    {
      const auto mode = next(argument);
      if (mode == "mapping")
        options.analysis_mode = AnalysisMode::Mapping;
      else if (mode == "hierarchy-rd")
        options.analysis_mode = AnalysisMode::HierarchyRd;
      else
        throw std::invalid_argument("unknown analysis mode: " + mode);
    }
    else if (argument == "--telemetry" || argument == "--export-events")
    {
      hierarchy_options = true;
      auto & path = argument == "--telemetry" ? options.telemetry_path
                                              : options.events_path;
      path = artifact_path(next(argument), argument);
    }
    else if (argument == "--event-limit" ||
             argument == "--max-single-loop-iterations" ||
             argument == "--max-cumulative-loop-iterations" ||
             argument == "--max-source-accesses" ||
             argument == "--max-line-references")
    {
      hierarchy_options = true;
      const auto value = unsigned_value(next(argument), argument);
      if (argument == "--event-limit")
      {
        event_limit_explicit = true;
        options.event_limit = value;
      }
      else if (argument == "--max-single-loop-iterations")
        options.loop_limits.single_loop_iterations = value;
      else if (argument == "--max-cumulative-loop-iterations")
        options.loop_limits.cumulative_loop_iterations = value;
      else if (argument == "--max-source-accesses")
        options.emission_limits.emitted_source_accesses = value;
      else
        options.emission_limits.emitted_line_references = value;
    }
    else if (argument == "--mode")
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
  if (options.analysis_mode != AnalysisMode::HierarchyRd && hierarchy_options)
    throw std::invalid_argument(
        "diagnostic and work-limit options require --analysis hierarchy-rd");
  if (options.analysis_mode != AnalysisMode::Legacy && options.elf_path.empty())
    throw std::invalid_argument("--elf is required for explicit analysis");
  if (options.analysis_mode == AnalysisMode::HierarchyRd)
  {
    if (options.export_path.empty())
      throw std::invalid_argument("--export is required for hierarchy-rd");
    artifact_path(options.export_path, "--export");
    if (event_limit_explicit && options.events_path.empty())
      throw std::invalid_argument("--event-limit requires --export-events");
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

} // namespace yarda::cli
