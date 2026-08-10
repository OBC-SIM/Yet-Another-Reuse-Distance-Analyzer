#pragma once

#include <nlohmann/json.hpp>

namespace yarda
{

/**
 * @brief Normalize legacy and APE v2 module roots to enriched function entries.
 *
 * @param raw Parsed LAT JSON root.
 * @return Function array with object metadata copied into array nodes.
 * @throws std::invalid_argument if the root schema is unsupported.
 */
nlohmann::json normalize_module(const nlohmann::json & raw);

}  // namespace yarda
