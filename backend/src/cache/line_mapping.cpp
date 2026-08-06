#include "yarda/cache/line_mapping.hpp"

#include <limits>
#include <stdexcept>

namespace yarda
{

CacheLineMapping map_cache_line(const std::string & object_id,
                                std::uint64_t object_byte_offset,
                                std::uint64_t access_size,
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
  return {object_id, object_byte_offset, objects.basis,
          decode_cache_address(address, geometry)};
}

}  // namespace yarda
