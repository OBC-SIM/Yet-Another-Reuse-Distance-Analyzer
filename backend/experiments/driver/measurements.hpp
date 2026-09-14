#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace yarda::evaluation
{

/**
 * @brief Read this executable image's Linux VmHWM, normalized to bytes.
 * @return Peak resident bytes since exec; throws when unavailable or invalid.
 * @note Unlike process-lifetime getrusage, this excludes a larger launcher
 * image before exec. It includes input parsing, analysis and serialization.
 */
std::uint64_t image_peak_rss_bytes();

/**
 * @brief Execute one prepared case in a fresh output directory.
 * @param case_file Prepared immutable manifest row.
 * @param mode batch, streaming, instrumented or verify.
 * @param output New, exclusively owned directory for this invocation.
 * @return Zero on success, one on classified failure; output errors throw.
 */
int measure_case(const std::string & case_file, const std::string & mode,
                 const std::string & output);

} // namespace yarda::evaluation
