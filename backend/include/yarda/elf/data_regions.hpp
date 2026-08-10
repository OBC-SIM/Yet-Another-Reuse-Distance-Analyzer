#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace yarda
{

/** @brief Byte order declared by an ELF image. */
enum class ElfEndianness
{
  Little,
  Big,
};

/** @brief Linked ELF image types accepted by the data-region parser. */
enum class ElfImageType
{
  Executable,
  SharedObject,
};

/** @brief Storage semantics of one allocated non-executable ELF region. */
enum class ElfDataRegionKind
{
  ReadOnly,
  Initialized,
  ZeroInitialized,
};

/** @brief ELF table used to derive a region. */
enum class ElfRegionOrigin
{
  Section,
  Segment,
};

/** @brief One allocated, non-executable data range in an ELF image. */
struct ElfDataRegion
{
  std::string name;
  ElfDataRegionKind kind = ElfDataRegionKind::ReadOnly;
  ElfRegionOrigin origin = ElfRegionOrigin::Section;
  std::uint64_t virtual_address = 0;
  std::uint64_t file_offset = 0;
  std::uint64_t file_size = 0;
  std::uint64_t memory_size = 0;
  std::uint64_t alignment = 0;
  std::uint64_t source_index = 0;
  bool readable = false;
  bool writable = false;
  bool executable = false;
};

/** @brief Object symbol associated with a parsed data region. */
struct ElfDataSymbol
{
  std::string name;
  std::uint64_t virtual_address = 0;
  std::uint64_t size = 0;
  std::size_t region_index = 0;
};

/** @brief Compiler-independent static data layout extracted from one ELF. */
struct ElfDataRegions
{
  std::uint8_t address_size = 0;
  ElfEndianness endianness = ElfEndianness::Little;
  ElfImageType image_type = ElfImageType::Executable;
  std::uint16_t machine = 0;
  bool image_relative = false;
  bool used_program_headers = false;
  std::vector<ElfDataRegion> regions;
  std::vector<ElfDataSymbol> symbols;
};

/**
 * @brief Parse static data regions and object symbols from a linked ELF file.
 *
 * Only allocated, non-executable storage is returned. Shared-object addresses
 * remain relative to the image load bias. If section headers are unavailable,
 * load program headers provide a region-only fallback. Thread-local storage is
 * excluded because its runtime address cannot be derived from the ELF alone.
 *
 * @param path Path to an ELF `ET_EXEC` or `ET_DYN` image.
 * @return Parsed image properties, regions, and mapped symbols.
 * @throws std::invalid_argument if the input is not a supported linked ELF.
 * @throws std::runtime_error if the file is unreadable or malformed.
 */
ElfDataRegions parse_elf_data_regions(const std::filesystem::path & path);

}  // namespace yarda
