#include "yarda/elf/data_regions.hpp"

#include <algorithm>
#include <llvm/BinaryFormat/ELF.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Error.h>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

#include "program_regions.hpp"

namespace yarda
{
namespace
{

using llvm::object::ELFObjectFileBase;
using llvm::object::ELFSectionRef;
using llvm::object::ELFSymbolRef;
using llvm::object::ObjectFile;
using llvm::object::SectionRef;
using llvm::object::SymbolRef;

ElfDataRegionKind classify_section(std::uint64_t flags, std::uint32_t type)
{
  const bool zero = type == llvm::ELF::SHT_NOBITS;
  if (zero)
  {
    return ElfDataRegionKind::ZeroInitialized;
  }
  return (flags & llvm::ELF::SHF_WRITE) != 0 ? ElfDataRegionKind::Initialized
                                             : ElfDataRegionKind::ReadOnly;
}

std::vector<ElfDataRegion> parse_sections(const ELFObjectFileBase & object)
{
  std::vector<ElfDataRegion> regions;
  for (const SectionRef & section : object.sections())
  {
    const ELFSectionRef elf_section(section);
    const auto flags = elf_section.getFlags();
    if ((flags & llvm::ELF::SHF_ALLOC) == 0 ||
        (flags & llvm::ELF::SHF_EXECINSTR) != 0 || section.getSize() == 0)
    {
      continue;
    }
    if ((flags & llvm::ELF::SHF_TLS) != 0)
    {
      continue;
    }
    auto name = section.getName();
    if (!name)
    {
      throw std::runtime_error(llvm::toString(name.takeError()));
    }
    ElfDataRegion region;
    region.name = name->str();
    region.kind = classify_section(flags, elf_section.getType());
    region.origin = ElfRegionOrigin::Section;
    region.virtual_address = section.getAddress();
    region.file_offset = elf_section.getOffset();
    region.file_size =
      elf_section.getType() == llvm::ELF::SHT_NOBITS ? 0 : section.getSize();
    region.memory_size = section.getSize();
    region.alignment = section.getAlignment();
    region.source_index = section.getIndex();
    region.readable = true;
    region.writable = (flags & llvm::ELF::SHF_WRITE) != 0;
    region.executable = false;
    regions.push_back(std::move(region));
  }
  std::sort(
    regions.begin(), regions.end(), [](const auto & left, const auto & right) {
      return std::tie(left.virtual_address, left.source_index, left.name) <
             std::tie(right.virtual_address, right.source_index, right.name);
    });
  return regions;
}

std::map<std::uint64_t, std::size_t>
index_regions(const std::vector<ElfDataRegion> & regions)
{
  std::map<std::uint64_t, std::size_t> result;
  for (std::size_t index = 0; index < regions.size(); ++index)
  {
    result.emplace(regions[index].source_index, index);
  }
  return result;
}

using SymbolKey =
  std::tuple<std::string, std::uint64_t, std::uint64_t, std::uint64_t>;

void append_symbol(const SymbolRef & raw,
                   const std::map<std::uint64_t, std::size_t> & region_indices,
                   std::set<SymbolKey> & seen,
                   std::vector<ElfDataSymbol> & symbols)
{
  const ELFSymbolRef symbol(raw);
  const auto type = symbol.getELFType();
  if (type != llvm::ELF::STT_OBJECT)
  {
    return;
  }
  auto section = raw.getSection();
  if (!section)
  {
    throw std::runtime_error(llvm::toString(section.takeError()));
  }
  if (*section == raw.getObject()->section_end())
  {
    return;
  }
  const auto region = region_indices.find((*section)->getIndex());
  if (region == region_indices.end())
  {
    return;
  }
  auto name = raw.getName();
  auto value = raw.getAddress();
  if (!name)
  {
    throw std::runtime_error(llvm::toString(name.takeError()));
  }
  if (!value)
  {
    throw std::runtime_error(llvm::toString(value.takeError()));
  }
  if (name->empty())
  {
    return;
  }
  SymbolKey key{name->str(), *value, symbol.getSize(), (*section)->getIndex()};
  if (!seen.insert(key).second)
  {
    return;
  }
  symbols.push_back({name->str(), *value, symbol.getSize(), region->second});
}

std::vector<ElfDataSymbol> parse_symbols(
  const ELFObjectFileBase & object, const std::vector<ElfDataRegion> & regions)
{
  const auto region_indices = index_regions(regions);
  std::vector<ElfDataSymbol> symbols;
  std::set<SymbolKey> seen;
  for (const auto & symbol : object.symbols())
  {
    append_symbol(symbol, region_indices, seen, symbols);
  }
  for (const auto & symbol : object.getDynamicSymbolIterators())
  {
    append_symbol(symbol, region_indices, seen, symbols);
  }
  std::sort(symbols.begin(), symbols.end(),
            [](const auto & left, const auto & right) {
              return std::tie(left.virtual_address, left.name) <
                     std::tie(right.virtual_address, right.name);
            });
  return symbols;
}

ElfImageType image_type(std::uint16_t type)
{
  if (type == llvm::ELF::ET_EXEC)
  {
    return ElfImageType::Executable;
  }
  if (type == llvm::ELF::ET_DYN)
  {
    return ElfImageType::SharedObject;
  }
  throw std::invalid_argument("ELF input must be ET_EXEC or ET_DYN");
}

}  // namespace

ElfDataRegions parse_elf_data_regions(const std::filesystem::path & path)
{
  std::error_code filesystem_error;
  if (!std::filesystem::exists(path, filesystem_error) || filesystem_error)
  {
    throw std::runtime_error("ELF input does not exist: " + path.string());
  }
  if (!std::filesystem::is_regular_file(path, filesystem_error) ||
      filesystem_error)
  {
    throw std::invalid_argument("ELF input must be a regular file: " +
                                path.string());
  }
  auto owning_object = ObjectFile::createObjectFile(path.string());
  if (!owning_object)
  {
    throw std::invalid_argument("input is not a valid object file: " +
                                llvm::toString(owning_object.takeError()));
  }
  const auto * object =
    llvm::dyn_cast<ELFObjectFileBase>(owning_object->getBinary());
  if (object == nullptr)
  {
    throw std::invalid_argument("input object is not ELF");
  }

  ElfDataRegions result;
  result.address_size = object->getBytesInAddress();
  result.endianness =
    object->isLittleEndian() ? ElfEndianness::Little : ElfEndianness::Big;
  result.image_type = image_type(object->getEType());
  result.machine = object->getEMachine();
  result.image_relative = result.image_type == ElfImageType::SharedObject;
  result.regions = parse_sections(*object);
  result.used_program_headers = result.regions.empty();
  if (result.used_program_headers)
  {
    result.regions = detail::parse_program_regions(*object);
  }
  else
  {
    result.symbols = parse_symbols(*object, result.regions);
  }
  return result;
}

}  // namespace yarda
