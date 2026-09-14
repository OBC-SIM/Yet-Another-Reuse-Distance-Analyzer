#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace yarda
{

/**
 * @brief Own the effective semantic identity of one hierarchy analysis.
 *
 * Hashes identify exact raw file bytes. The caller supplies a build-fixed
 * commit/release version and every effective result-changing option, including
 * defaults. The current model has no such options, so its object is empty.
 * Work allowances, diagnostic limits and output paths are not semantic options.
 */
struct AnalysisIdentityInput
{
  std::string tool_version;
  std::string lat_sha256;
  std::string elf_sha256;
  std::string cache_config_sha256;
  std::uint32_t analysis_core_id = 0;
  nlohmann::json semantic_analysis_options = nlohmann::json::object();
};

/**
 * @brief Hash a file's exact bytes without retaining the complete file.
 * @param path Borrowed input path; no normalization of contents is performed.
 * @return Lowercase, 64-character SHA-256 hexadecimal digest.
 * @throws std::runtime_error if opening or reading the file fails.
 */
std::string sha256_file_bytes(const std::string & path);

/**
 * @brief Hash the compact, lexicographically ordered RESULT v2 preimage.
 *
 * The result schema version separates v1 and v2 identities. EVENTS and
 * TELEMETRY share this identity while retaining their own schema versions.
 * @param input Borrowed effective identity; option leaves must be integers,
 * booleans or strings, with object keys sorted and array order preserved.
 * @return Lowercase SHA-256 digest, independent of insertion order.
 * @throws std::invalid_argument for incomplete identity, nonzero core, invalid
 * hash syntax or options containing null, floating-point or binary values.
 */
std::string hierarchy_analysis_id(const AnalysisIdentityInput & input);

} // namespace yarda
