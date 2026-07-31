#include "elf_program_regions.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <llvm/BinaryFormat/ELF.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Error.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace yarda::detail
{
namespace
{

using llvm::object::ELF32BEObjectFile;
using llvm::object::ELF32LEObjectFile;
using llvm::object::ELF64BEObjectFile;
using llvm::object::ELF64LEObjectFile;

struct HeaderRange
{
  std::uint64_t start;
  std::uint64_t file_end;
  std::uint64_t memory_end;
  std::uint64_t file_offset;
  std::uint64_t alignment;
  std::uint64_t index;
  std::uint32_t flags;
};

std::uint64_t checked_add(std::uint64_t left, std::uint64_t right,
                          const char * context)
{
  if (right > std::numeric_limits<std::uint64_t>::max() - left)
  {
    throw std::runtime_error(std::string("invalid ELF ") + context);
  }
  return left + right;
}

bool overlaps_tls(std::uint64_t start, std::uint64_t end,
                  const std::vector<HeaderRange> & tls_headers)
{
  for (const auto & tls : tls_headers)
  {
    if (start >= tls.start && end <= tls.memory_end)
    {
      return true;
    }
  }
  return false;
}

ElfDataRegion make_region(const HeaderRange & load, std::uint64_t start,
                          std::uint64_t end, std::size_t part)
{
  const bool file_backed = start < load.file_end;
  ElfDataRegion result;
  result.name = "PT_LOAD[" + std::to_string(load.index) + "].part[" +
                std::to_string(part) + "]";
  result.kind = file_backed ? ((load.flags & llvm::ELF::PF_W) != 0
                                 ? ElfDataRegionKind::Initialized
                                 : ElfDataRegionKind::ReadOnly)
                            : ElfDataRegionKind::ZeroInitialized;
  result.origin = ElfRegionOrigin::Segment;
  result.virtual_address = start;
  result.file_offset =
    checked_add(load.file_offset, start - load.start, "segment offset");
  result.file_size = file_backed ? end - start : 0;
  result.memory_size = end - start;
  result.alignment = load.alignment;
  result.source_index = load.index;
  result.readable = (load.flags & llvm::ELF::PF_R) != 0;
  result.writable = (load.flags & llvm::ELF::PF_W) != 0;
  result.executable = false;
  return result;
}

void append_load(const HeaderRange & load,
                 const std::vector<HeaderRange> & tls_headers,
                 std::vector<ElfDataRegion> & regions)
{
  std::vector<std::uint64_t> boundaries = {load.start, load.file_end,
                                           load.memory_end};
  for (const auto & tls : tls_headers)
  {
    if (tls.start < load.memory_end && tls.memory_end > load.start)
    {
      boundaries.push_back(std::max(load.start, tls.start));
      boundaries.push_back(
        std::max(load.start, std::min(load.memory_end, tls.memory_end)));
    }
  }
  std::sort(boundaries.begin(), boundaries.end());
  boundaries.erase(std::unique(boundaries.begin(), boundaries.end()),
                   boundaries.end());
  std::size_t part = 0;
  for (std::size_t index = 1; index < boundaries.size(); ++index)
  {
    const auto start = boundaries[index - 1];
    const auto end = boundaries[index];
    if (start >= end)
    {
      continue;
    }
    if (!overlaps_tls(start, end, tls_headers))
    {
      regions.push_back(make_region(load, start, end, part++));
    }
  }
}

template <typename Object>
std::vector<ElfDataRegion> parse_typed(const Object & object)
{
  auto headers = object.getELFFile().program_headers();
  if (!headers)
  {
    throw std::runtime_error(llvm::toString(headers.takeError()));
  }

  std::vector<HeaderRange> loads;
  std::vector<HeaderRange> tls_headers;
  std::uint64_t index = 0;
  const auto object_size = object.getData().size();
  for (const auto & header : *headers)
  {
    const bool is_load = header.p_type == llvm::ELF::PT_LOAD;
    const bool is_tls = header.p_type == llvm::ELF::PT_TLS;
    if (!is_load && !is_tls)
    {
      ++index;
      continue;
    }
    const auto file_end =
      checked_add(header.p_vaddr, header.p_filesz, "segment file range");
    const auto memory_end =
      checked_add(header.p_vaddr, header.p_memsz, "segment memory range");
    const auto offset_end =
      checked_add(header.p_offset, header.p_filesz, "segment file offset");
    if (header.p_filesz > header.p_memsz || offset_end > object_size)
    {
      throw std::runtime_error("invalid ELF segment extent");
    }
    const HeaderRange range{header.p_vaddr,  file_end,       memory_end,
                            header.p_offset, header.p_align, index,
                            header.p_flags};
    if (is_tls && header.p_memsz != 0)
    {
      tls_headers.push_back(range);
    }
    if (is_load && header.p_memsz != 0 &&
        (header.p_flags & llvm::ELF::PF_X) == 0)
    {
      loads.push_back(range);
    }
    ++index;
  }

  std::vector<ElfDataRegion> regions;
  for (const auto & load : loads)
  {
    append_load(load, tls_headers, regions);
  }
  std::sort(
    regions.begin(), regions.end(), [](const auto & left, const auto & right) {
      return std::tie(left.virtual_address, left.source_index, left.name) <
             std::tie(right.virtual_address, right.source_index, right.name);
    });
  return regions;
}

}  // namespace

std::vector<ElfDataRegion>
parse_program_regions(const llvm::object::ELFObjectFileBase & object)
{
  if (const auto * typed = llvm::dyn_cast<ELF32LEObjectFile>(&object))
  {
    return parse_typed(*typed);
  }
  if (const auto * typed = llvm::dyn_cast<ELF64LEObjectFile>(&object))
  {
    return parse_typed(*typed);
  }
  if (const auto * typed = llvm::dyn_cast<ELF32BEObjectFile>(&object))
  {
    return parse_typed(*typed);
  }
  if (const auto * typed = llvm::dyn_cast<ELF64BEObjectFile>(&object))
  {
    return parse_typed(*typed);
  }
  throw std::invalid_argument("unsupported ELF class or byte order");
}

}  // namespace yarda::detail
