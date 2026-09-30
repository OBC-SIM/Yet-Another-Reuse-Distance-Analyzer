#pragma once

#include <fstream>
#include <iterator>
#include <string>

#include "cli/hierarchy_command.hpp"
#include "cache/output/temporary_input.hpp"
#include "trace/output/task_access_stream_test_support.hpp"

namespace yarda::test::cli
{

using Json = nlohmann::json;
using yarda::AnalysisTelemetryHost;
namespace fs = std::filesystem;
using artifact::TemporaryInput;
using namespace yarda::test::stream;

inline void write(const fs::path & path, const std::string & text)
{
  std::ofstream file;
  file.exceptions(std::ios::failbit | std::ios::badbit);
  file.open(path, std::ios::binary);
  file << text;
  file.close();
}

inline std::string read(const fs::path & path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("missing test artifact: " + path.string());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

inline Json raw()
{
  const auto reference = access("global::yarda_mapped_value", "0", "store");
  return {{"schema_version", 2},
          {"metadata",
           {{"objects",
             {{"global::yarda_mapped_value",
               {{"kind", "array"}, {"shape", {1}}, {"elem_size", 8}}}}}}},
          {"functions", Json::array({function("first", {loop(2, {reference})}),
                                     function("second", {loop(2, {reference})}),
                                     function("empty", Json::array())})}};
}

class HierarchyCommandTest : public ::testing::Test
{
protected:
  TemporaryInput temporary;
  yarda::cli::Options options;

  void SetUp() override
  {
    options.analysis_mode = yarda::cli::AnalysisMode::HierarchyRd;
    options.input = temporary.path;
    options.cache_path = (temporary.directory / "cache.yaml").string();
    options.elf_path = YARDA_ELF_EXEC_FIXTURE_PATH;
    options.export_path = (temporary.directory / "result.json").string();
    write(options.input, raw().dump());
    write(options.cache_path, read(YARDA_CLI_CACHE_PATH));
  }

  void diagnostics(std::uint64_t limit = 8)
  {
    options.events_path = (temporary.directory / "events.json").string();
    options.telemetry_path = (temporary.directory / "telemetry.json").string();
    options.event_limit = limit;
  }

  void expect_no_outputs() const
  {
    EXPECT_FALSE(fs::exists(options.export_path));
    if (!options.events_path.empty())
    {
      EXPECT_FALSE(fs::exists(options.events_path));
    }
    if (!options.telemetry_path.empty())
    {
      EXPECT_FALSE(fs::exists(options.telemetry_path));
    }
    for (const auto & entry : fs::directory_iterator(temporary.directory))
      EXPECT_EQ(entry.path().filename().string().find(".yarda-json-"),
                std::string::npos);
  }
};

struct Measurements
{
  std::uint64_t ticks = 0;
  AnalysisTelemetryProviders providers()
  {
    return {[this]
            {
              ticks += 10;
              return ticks;
            },
            [] { return 4096; },
            [] {
              return AnalysisTelemetryHost{"host", "Linux", "test", "x86_64"};
            },
            [] { return "2026-09-13T00:00:00Z"; }};
  }
};

inline yarda::cli::Options parse(std::vector<std::string> arguments)
{
  arguments.insert(arguments.begin(), "yarda_cpp");
  std::vector<char *> argv;
  for (auto & argument : arguments)
    argv.push_back(argument.data());
  return yarda::cli::parse_options(static_cast<int>(argv.size()), argv.data());
}

} // namespace yarda::test::cli
