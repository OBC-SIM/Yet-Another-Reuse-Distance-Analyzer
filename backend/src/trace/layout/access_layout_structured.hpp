#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "access_layout.hpp"

namespace yarda::detail
{

ByteAccess resolve_structured_access(const nlohmann::json & node,
                                     const std::vector<std::string> & indices,
                                     const nlohmann::json & objects,
                                     const nlohmann::json & structures);

/**
 * @brief Record layout steps while validating the first numeric path in order.
 * @param node Borrowed immutable access and original path expressions.
 * @param indices Borrowed numeric row with the node's index rank.
 * @param objects Borrowed object metadata after inline binding.
 * @param structures Borrowed ABI structure layouts.
 * @param plan Scratch output, usable only after successful return.
 * @return Exact byte range selected by the first access.
 * @throws std::invalid_argument or JSON exceptions at the original check site.
 */
ByteAccess prepare_structured_access(const nlohmann::json & node,
                                     const std::vector<std::int64_t> & indices,
                                     const nlohmann::json & objects,
                                     const nlohmann::json & structures,
                                     PreparedLayout & plan);

}  // namespace yarda::detail
