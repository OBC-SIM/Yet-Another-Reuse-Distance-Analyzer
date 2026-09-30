#pragma once

#include "yarda/cache/analysis_telemetry.hpp"

namespace yarda::detail
{

/** @brief Normalize Linux getrusage KiB with checked byte arithmetic. */
std::uint64_t linux_peak_rss_bytes(std::int64_t kibibytes);

/** @brief Supply private system adapters, keeping their headers out of APIs. */
AnalysisTelemetryProviders runtime_measurement_providers();

} // namespace yarda::detail
