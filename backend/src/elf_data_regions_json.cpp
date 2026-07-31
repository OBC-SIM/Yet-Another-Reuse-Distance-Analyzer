#include "yarda/elf_data_regions_json.hpp"

#include <stdexcept>
#include <string_view>

namespace yarda
{
namespace
{

std::string_view to_string(ElfEndianness value)
{
  switch (value)
  {
    case ElfEndianness::Little:
      return "little";
    case ElfEndianness::Big:
      return "big";
  }
  throw std::logic_error("unknown ELF endianness");
}

std::string_view to_string(ElfImageType value)
{
  switch (value)
  {
    case ElfImageType::Executable:
      return "executable";
    case ElfImageType::SharedObject:
      return "shared-object";
  }
  throw std::logic_error("unknown ELF image type");
}

std::string_view to_string(ElfDataRegionKind value)
{
  switch (value)
  {
    case ElfDataRegionKind::ReadOnly:
      return "read-only";
    case ElfDataRegionKind::Initialized:
      return "initialized";
    case ElfDataRegionKind::ZeroInitialized:
      return "zero-initialized";
  }
  throw std::logic_error("unknown ELF data-region kind");
}

std::string_view to_string(ElfRegionOrigin value)
{
  switch (value)
  {
    case ElfRegionOrigin::Section:
      return "section";
    case ElfRegionOrigin::Segment:
      return "segment";
  }
  throw std::logic_error("unknown ELF region origin");
}

nlohmann::json region_json(const ElfDataRegion & region)
{
  return {
    {"name", region.name},
    {"kind", to_string(region.kind)},
    {"origin", to_string(region.origin)},
    {"virtual_address", region.virtual_address},
    {"file_offset", region.file_offset},
    {"file_size", region.file_size},
    {"memory_size", region.memory_size},
    {"alignment", region.alignment},
    {"source_index", region.source_index},
    {"permissions",
     {{"read", region.readable},
      {"write", region.writable},
      {"execute", region.executable}}},
  };
}

nlohmann::json symbol_json(const ElfDataSymbol & symbol)
{
  return {
    {"name", symbol.name},
    {"virtual_address", symbol.virtual_address},
    {"size", symbol.size},
    {"region_index", symbol.region_index},
  };
}

}  // namespace

nlohmann::json elf_data_regions_json(const ElfDataRegions & image)
{
  nlohmann::json regions = nlohmann::json::array();
  for (const auto & region : image.regions)
  {
    regions.push_back(region_json(region));
  }
  nlohmann::json symbols = nlohmann::json::array();
  for (const auto & symbol : image.symbols)
  {
    symbols.push_back(symbol_json(symbol));
  }
  return {
    {"schema_version", 1},
    {"image",
     {{"address_size", image.address_size},
      {"endianness", to_string(image.endianness)},
      {"type", to_string(image.image_type)},
      {"machine", image.machine},
      {"relative_addresses", image.image_relative},
      {"program_header_fallback", image.used_program_headers}}},
    {"regions", std::move(regions)},
    {"symbols", std::move(symbols)},
  };
}

}  // namespace yarda
