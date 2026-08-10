#pragma once

#include "yarda/elf/address_model.hpp"
#include "yarda/elf/data_regions.hpp"

namespace yarda
{

/**
 * @brief Convert ELF object symbols to canonical global LAT object IDs.
 *
 * Identical symbol-table duplicates are collapsed. A name that resolves to
 * different storage is ambiguous and rejected.
 *
 * @param image Parsed linked ELF data regions and symbols.
 * @return Address model keyed by `global::<ELF symbol name>`.
 * @throws std::invalid_argument if symbols are unavailable or ambiguous.
 */
ObjectAddressModel build_elf_object_addresses(const ElfDataRegions & image);

}  // namespace yarda
