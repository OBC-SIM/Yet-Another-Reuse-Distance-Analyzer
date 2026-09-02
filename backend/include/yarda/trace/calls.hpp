#pragma once

#include <nlohmann/json.hpp>

namespace yarda
{

/**
 * @brief Expand known direct calls and retain legacy report roots.
 *
 * @param raw Legacy or APE v2 LAT module.
 * @return Normalized function entries without Call nodes.
 * @throws std::invalid_argument for empty or duplicate function identities,
 * unknown callees, recursion, or argument count mismatches.
 */
nlohmann::json expand_calls(const nlohmann::json & raw);

/**
 * @brief Expand inline-annotated calls under explicitly analyzed task roots.
 *
 * Calls to known module functions without `ape.inline` or `yard.inline` are
 * treated as opaque and excluded from the task trace. Unknown targets are
 * rejected. At least one function must carry `ape.analyze` or `yard.analyze`;
 * this API has no legacy all-functions fallback.
 *
 * @param raw APE v2 LAT module.
 * @return Analyzed root functions with eligible direct calls expanded.
 * @throws std::invalid_argument if no analyzed root exists, if one function
 * has both analyze and inline roles, or for empty or duplicate function
 * identities, unknown targets, recursion, or argument count mismatches in an
 * expanded inline call.
 */
nlohmann::json expand_task_calls(const nlohmann::json & raw);

}  // namespace yarda
