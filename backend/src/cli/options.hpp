#pragma once

#include <string>
#include "yarda/trace/emission_budget.hpp"
#include "yarda/trace/trace.hpp"
#include "yarda/trace/work_limits.hpp"

namespace yarda::cli
{

/** @brief Legacy dispatch remains implicit when --analysis is omitted. */
enum class AnalysisMode
{
  Legacy,
  Mapping,
  HierarchyRd
};

/** @brief Own the arguments for one CLI invocation. */
struct Options
{
  AnalysisMode analysis_mode = AnalysisMode::Legacy;
  std::string input;
  yarda::Granularity granularity = yarda::Granularity::Element;
  bool granularity_explicit = false;
  std::string cache_path;
  std::string elf_path;
  std::string export_path;
  std::string telemetry_path;
  std::string events_path;
  std::uint64_t event_limit = 0;
  LoopWorkLimits loop_limits;
  TraceEmissionLimits emission_limits;
};

/**
 * @brief Print the command's accepted arguments to stdout.
 * @return Nothing.
 */
void print_usage();

/**
 * @brief Parse one invocation, preserving legacy dispatch and option precedence.
 * @param argc Number of arguments, including the executable name.
 * @param argv Borrowed non-null argument vector; strings are copied.
 * @return Owned arguments; help prints usage and exits successfully.
 * @throws std::invalid_argument for an unsupported argument combination.
 */
Options parse_options(int argc, char ** argv);

} // namespace yarda::cli
