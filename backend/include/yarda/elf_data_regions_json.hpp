#pragma once

#include <nlohmann/json.hpp>

#include "yarda/elf_data_regions.hpp"

namespace yarda
{

/**
 * @brief Serialize parsed ELF data regions using the versioned JSON contract.
 *
 * Numeric addresses and sizes remain byte-valued unsigned integers. Enum
 * values use stable kebab-case strings suitable for downstream consumers.
 *
 * @param image Parsed ELF data-region model.
 * @return JSON object with schema version, image metadata, regions, symbols.
 */
nlohmann::json elf_data_regions_json(const ElfDataRegions & image);

}  // namespace yarda
