#pragma once

#include <nlohmann/json.hpp>

namespace yarda
{

/**
 * @brief Expand annotated direct calls and retain analyzed root functions.
 *
 * @param raw Legacy or APE v2 LAT module.
 * @return Normalized function entries without Call nodes.
 * @throws std::invalid_argument for unknown callees, recursion, or argument
 * count mismatches.
 */
nlohmann::json expand_calls(const nlohmann::json & raw);

}  // namespace yarda
