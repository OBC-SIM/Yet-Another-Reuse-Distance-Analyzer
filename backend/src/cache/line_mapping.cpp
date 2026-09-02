#include "yarda/cache/line_mapping.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace yarda
{
std::vector<CacheLineMapping> map_cache_lines(
  const CacheLineAddressRange & range, const CacheGeometry & geometry)
{
  if (range.object_id.empty() || range.access_size == 0)
  {
    throw std::invalid_argument("cache-line address range is invalid");
  }
  if (range.access_size - 1 >
      std::numeric_limits<std::uint64_t>::max() - range.linked_byte_address)
  {
    throw std::overflow_error("linked address range overflows");
  }
  if (range.access_size - 1 >
      std::numeric_limits<std::uint64_t>::max() - range.object_byte_offset)
  {
    throw std::overflow_error("object offset range overflows");
  }

  std::vector<CacheLineMapping> mappings;
  auto current_address = range.linked_byte_address;
  auto current_object_offset = range.object_byte_offset;
  auto remaining = range.access_size;
  std::uint64_t line_span_ordinal = 0;
  while (remaining != 0)
  {
    CacheLineMapping mapping;
    mapping.object_id = range.object_id;
    mapping.object_byte_offset = current_object_offset;
    mapping.address_basis = range.address_basis;
    mapping.decoded = decode_cache_address(current_address, geometry);
    mapping.source_object_byte_offset = range.object_byte_offset;
    mapping.source_access_size = range.access_size;
    mapping.source_linked_byte_address = range.linked_byte_address;
    mapping.line_span_ordinal = line_span_ordinal++;
    mappings.push_back(std::move(mapping));

    const auto available =
      geometry.line_size - mappings.back().decoded.line_offset;
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
