#pragma once

#include <nlohmann/json.hpp>

#include "yarda/cache/line_mapping.hpp"

namespace yarda
{

/**
 * @brief Serialize deterministic cache-line mapping rows for reports.
 *
 * @param mappings Mapping table keyed by object ID and object byte offset.
 * @return JSON array containing address, Tag, Index, and Offset fields.
 */
nlohmann::json cache_line_mapping_json(const CacheLineMappingTable & mappings);

}  // namespace yarda
