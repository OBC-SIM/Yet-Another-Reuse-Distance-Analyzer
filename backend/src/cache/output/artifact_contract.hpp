#pragma once

#include <stdexcept>
#include <string_view>

namespace yarda::detail
{

inline constexpr auto kArtifactSchemaVersion = 1;
inline constexpr auto kAnalysisMode = "hierarchy-rd";
inline constexpr auto kModelId = "exact-two-level-lru-demand-v1";
inline constexpr auto kCsrdMode = "full-exact";
inline constexpr auto kAddressBasis = "linked_absolute";

/** @brief Reject noncanonical raw-input or analysis digests. */
inline void require_sha256(std::string_view value)
{
  if (value.size() != 64)
    throw std::invalid_argument("artifact requires a 64-character SHA-256");
  for (const auto digit : value)
    if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f')))
      throw std::invalid_argument("artifact SHA-256 must be lowercase hex");
}

} // namespace yarda::detail
