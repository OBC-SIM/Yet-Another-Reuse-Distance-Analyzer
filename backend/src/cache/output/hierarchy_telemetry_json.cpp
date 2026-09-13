#include "yarda/cache/analysis_telemetry.hpp"

#include "analysis_telemetry_validation.hpp"
#include "artifact_contract.hpp"

namespace yarda
{

nlohmann::ordered_json
hierarchy_telemetry_json(const AnalysisTelemetry & telemetry)
{
  detail::validate_analysis_telemetry(telemetry);
  auto stages = nlohmann::ordered_json::object();
  for (std::size_t i = 0; i < telemetry.stage_time_ns.size(); ++i)
    stages[detail::kAnalysisStageNames[i]] = *telemetry.stage_time_ns[i];
  return {{"schema_version", detail::kArtifactSchemaVersion},
          {"analysis_id", telemetry.analysis_id},
          {"total_time_ns", telemetry.total_time_ns},
          {"stage_time_ns", std::move(stages)},
          {"peak_rss_bytes", telemetry.peak_rss_bytes},
          {"source_accesses_emitted", telemetry.source_accesses_emitted},
          {"line_references_emitted", telemetry.line_references_emitted},
          {"maximum_inline_depth", telemetry.execution.maximum_inline_depth},
          {"loop_iterations_expanded",
           telemetry.execution.loop_iterations_expanded},
          {"host",
           {{"hostname", telemetry.host.hostname},
            {"os", telemetry.host.os},
            {"release", telemetry.host.release},
            {"machine", telemetry.host.machine}}},
          {"measured_at_utc", telemetry.measured_at_utc}};
}

} // namespace yarda
