#include "measurements.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

#include "evaluation.hpp"
#include "cache/output/runtime_measurement.hpp"
#include "cli/artifact_output.hpp"
#include "cache/rd/exact_csrd_arithmetic.hpp"

namespace yarda::evaluation
{

std::uint64_t image_peak_rss_bytes()
{
  std::ifstream file("/proc/self/status");
  std::string line;
  while (std::getline(file, line))
  {
    if (line.rfind("VmHWM:", 0) != 0) continue;
    std::istringstream value(line.substr(6));
    std::uint64_t kibibytes;
    std::string unit;
    if (!(value >> kibibytes >> unit) || unit != "kB" ||
        kibibytes > std::numeric_limits<std::uint64_t>::max() / 1024)
      throw std::runtime_error("invalid process VmHWM");
    return kibibytes * 1024;
  }
  throw std::runtime_error("process VmHWM is unavailable");
}

int measure_case(const std::string & case_file, const std::string & mode,
                 const std::string & output)
{
  if (mode != "batch" && mode != "streaming" && mode != "instrumented" &&
      mode != "verify") throw std::invalid_argument("unknown evaluation mode");
  if (!std::filesystem::create_directory(output))
    throw std::invalid_argument("evaluation output directory must be new: " + output);
  const auto provider = detail::runtime_measurement_providers();
  const auto start = provider.now_ns();
  nlohmann::json measurement{{"schema_version", 1}, {"mode", mode},
                            {"case_file", case_file}, {"status", "error"}};
  std::vector<cli::JsonArtifact> artifacts;
  std::vector<std::string> input_paths{case_file};
  bool success = false;
  try
  {
    const auto row = read_json(case_file);
    measurement["case_id"] = row.at("case_id");
    measurement["binary_sha256"] = sha256_file_bytes("/proc/self/exe");
    auto options = work_options(row);
    const auto inputs = read_inputs(row);
    for (const auto * key : {"lat_path", "elf_path", "cache_path"})
      input_paths.push_back(row.at(key).get<std::string>());
    const auto parsed = provider.now_ns();
    measurement["input_time_ns"] = parsed - start;
    measurement["state"] = nlohmann::json::array();
    if (mode == "instrumented")
    {
      measurement["compaction_count"] = 0;
      measurement["compaction_time_ns"] = 0;
      options.statistics_sink = [&](const auto & id, const auto & l1, const auto & llc) {
        measurement["state"].push_back({{"task_id", id},
          {"l1", statistics_json(l1)}, {"llc", statistics_json(llc)}});
        for (const auto & state : {l1, llc})
        {
          measurement["compaction_count"] = detail::csrd_checked_add(
            measurement["compaction_count"].template get<std::uint64_t>(), state.compaction_count);
          measurement["compaction_time_ns"] = detail::csrd_checked_add(
            measurement["compaction_time_ns"].template get<std::uint64_t>(), state.compaction_time_ns);
        }
      };
    }
    StreamingHierarchyResult result;
    if (mode == "verify")
    {
      verify_sources(inputs, row);
      verify_paths(inputs, options);
    }
    if (mode == "batch")
      result = batch_summary(run_batch(inputs.raw, inputs.objects,
                                       inputs.metadata.hierarchy, options));
    else
      result = analyze_streaming_hierarchy(inputs.raw, inputs.objects,
                                            inputs.metadata.hierarchy, options);
    const auto analyzed = provider.now_ns();
    measurement["analysis_time_ns"] = analyzed - parsed;
    auto document = hierarchy_result_json(inputs.metadata, result);
    auto text = document.dump(2);
    const auto serialized = provider.now_ns();
    measurement["serialize_time_ns"] = serialized - analyzed;
    measurement["total_time_ns"] = serialized - start;
    measurement["peak_rss_bytes"] = image_peak_rss_bytes();
    measurement["rusage_peak_rss_bytes"] = provider.peak_rss_bytes();
    measurement["analysis_id"] = document.at("analysis_id");
    measurement["tool_version"] = inputs.metadata.identity.tool_version;
    measurement["source_accesses"] = result.coverage.source_accesses;
    measurement["line_references"] = result.coverage.emitted_line_references;
    measurement["tasks"] = nlohmann::json::array();
    for (const auto & task : result.tasks)
      measurement["tasks"].push_back({{"task_id", task.task_id},
        {"distinct_l1_lines", task.l1.unique_lines},
        {"distinct_llc_lines", task.llc.unique_lines}});
    if (row.contains("expected_sources") &&
        row.at("expected_sources") != result.coverage.source_accesses)
      throw std::logic_error("unexpected completed source count");
    artifacts.push_back({output + "/result.json", std::move(text)});
    measurement["status"] = "success";
    success = true;
  }
  catch (const std::exception & error)
  {
    artifacts.clear();
    measurement["status"] = failure_status(error);
    measurement["error"] = error.what();
    measurement.erase("state");
    measurement.erase("compaction_count");
    measurement.erase("compaction_time_ns");
  }
  const auto host = provider.host();
  measurement["host"] = {{"hostname", host.hostname}, {"os", host.os},
                          {"release", host.release}, {"machine", host.machine}};
  measurement["measured_at_utc"] = provider.measured_at_utc();
  auto text = measurement.dump(2);
  artifacts.insert(artifacts.begin(), {output + "/measurement.json", std::move(text)});
  cli::publish_json_artifacts(input_paths, artifacts);
  return success ? 0 : 1;
}

} // namespace yarda::evaluation
