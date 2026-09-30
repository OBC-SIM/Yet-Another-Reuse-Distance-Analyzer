#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

#include "access_layout.hpp"

namespace yarda::detail
{

std::optional<ByteAccess> resolve_legacy_access(
  const nlohmann::json & node, const std::vector<std::string> & indices);

/**
 * @brief Resolve numeric legacy indices with their original flattening rules.
 * @param node Borrowed immutable access metadata.
 * @param indices Borrowed current numeric row.
 * @param plan Nullable borrowed scratch output; retained only on success.
 * @return Exact bytes, or no value for an unavailable/overflowing layout.
 */
std::optional<ByteAccess> prepare_legacy_access(
  const nlohmann::json & node, const std::vector<std::int64_t> & indices,
  PreparedLayout * plan);

}  // namespace yarda::detail
