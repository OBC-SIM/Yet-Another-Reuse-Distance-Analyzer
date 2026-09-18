#pragma once

#include "yarda/cache/analysis_telemetry.hpp"

namespace yarda::detail
{

inline constexpr std::array<const char *, 6> kAnalysisStageNames{
    "parse_map",          "parse_cache",        "parse_elf",
    "resolve_and_stream", "hierarchy_analysis", "serialize_result"};

/** @brief Reject overflow while accumulating measured nanoseconds. */
std::uint64_t checked_measurement_sum(std::uint64_t left, std::uint64_t right);

/** @brief Validate actual intervals, counter coherence and snapshot metadata.
 */
void validate_analysis_telemetry(const AnalysisTelemetry & telemetry);

} // namespace yarda::detail
