#include "analysis_telemetry_validation.hpp"

#include <limits>
#include <stdexcept>

#include "artifact_contract.hpp"

namespace yarda::detail
{
namespace
{

bool valid_timestamp(const std::string & value)
{
  if (value.size() != 20 || value[4] != '-' || value[7] != '-' ||
      value[10] != 'T' || value[13] != ':' || value[16] != ':' ||
      value[19] != 'Z')
    return false;
  for (std::size_t i = 0; i < value.size(); ++i)
  {
    if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19) continue;
    if (value[i] < '0' || value[i] > '9') return false;
  }
  const auto year = std::stoi(value.substr(0, 4));
  const auto month = std::stoi(value.substr(5, 2));
  const auto day = std::stoi(value.substr(8, 2));
  constexpr std::array<int, 12> days{31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return false;
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  const auto maximum = days[month - 1] + (month == 2 && leap ? 1 : 0);
  return day >= 1 && day <= maximum && std::stoi(value.substr(11, 2)) < 24 &&
         std::stoi(value.substr(14, 2)) < 60 &&
         std::stoi(value.substr(17, 2)) < 60;
}

} // namespace

std::uint64_t checked_measurement_sum(std::uint64_t left, std::uint64_t right)
{
  if (right > std::numeric_limits<std::uint64_t>::max() - left)
    throw std::overflow_error("telemetry duration overflows uint64_t");
  return left + right;
}

void validate_analysis_telemetry(const AnalysisTelemetry & telemetry)
{
  require_sha256(telemetry.analysis_id);
  const auto & stages = telemetry.stage_time_ns;
  std::uint64_t exclusive = 0;
  for (std::size_t i = 0; i < stages.size(); ++i)
  {
    if (!stages[i])
      throw std::invalid_argument("telemetry stage was not measured: " +
                                  std::string(kAnalysisStageNames[i]));
    if (i != static_cast<std::size_t>(AnalysisStage::HierarchyAnalysis))
      exclusive = checked_measurement_sum(exclusive, *stages[i]);
  }
  if (exclusive > telemetry.total_time_ns ||
      *stages[static_cast<std::size_t>(AnalysisStage::HierarchyAnalysis)] >
          *stages[static_cast<std::size_t>(AnalysisStage::ResolveAndStream)])
    throw std::invalid_argument(
        "telemetry intervals exceed their enclosing time");
  if (telemetry.source_accesses_emitted > telemetry.line_references_emitted ||
      (telemetry.source_accesses_emitted == 0 &&
       telemetry.line_references_emitted != 0))
    throw std::invalid_argument("telemetry source and line counts disagree");
  if (telemetry.host.hostname.empty() || telemetry.host.os.empty() ||
      telemetry.host.release.empty() || telemetry.host.machine.empty() ||
      !valid_timestamp(telemetry.measured_at_utc))
    throw std::invalid_argument("telemetry host or UTC timestamp is invalid");
}

} // namespace yarda::detail
