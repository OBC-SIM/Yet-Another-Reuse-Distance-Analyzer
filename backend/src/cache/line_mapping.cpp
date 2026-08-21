#include "yarda/cache/line_mapping.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace yarda
{

std::vector<CacheLineMapping>
map_cache_lines(const std::string & object_id,
                std::uint64_t object_byte_offset, std::uint64_t access_size,
                const ObjectAddressModel & objects,
                const CacheGeometry & geometry)
{
  const auto object = objects.objects.find(object_id);
  if (object == objects.objects.end())
  {
    throw std::invalid_argument("ELF object is unresolved: " + object_id);
  }
  if (access_size == 0 || object_byte_offset >= object->second.size ||
      access_size > object->second.size - object_byte_offset)
  {
    throw std::invalid_argument("access exceeds ELF object extent: " +
                                object_id);
  }
  if (object->second.base >
      std::numeric_limits<std::uint64_t>::max() - object_byte_offset)
  {
    throw std::overflow_error("ELF object address overflow: " + object_id);
  }
  const auto address = object->second.base + object_byte_offset;
  if (access_size - 1 >
      std::numeric_limits<std::uint64_t>::max() - address)
  {
    throw std::overflow_error("ELF object address overflow: " + object_id);
  }

  std::vector<CacheLineMapping> mappings;
  auto current_address = address;
  auto current_object_offset = object_byte_offset;
  auto remaining = access_size;
  while (remaining != 0)
  {
    const auto decoded = decode_cache_address(current_address, geometry);
    mappings.push_back(
      {object_id, current_object_offset, objects.basis, decoded});

    const auto available = geometry.line_size - decoded.line_offset;
    const auto consumed = std::min(remaining, available);
    remaining -= consumed;
    if (remaining != 0)
    {
      current_address += consumed;
      current_object_offset += consumed;
    }
  }
  return mappings;
}

}  // namespace yarda
