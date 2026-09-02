#include "yarda/trace/resolved_mapping.hpp"

namespace yarda
{

std::vector<CacheLineMapping> map_cache_lines(const ResolvedAccess & access,
                                              const CacheGeometry & geometry)
{
  auto mappings = map_cache_lines(
    CacheLineAddressRange{access.object_id, access.object_byte_offset,
                          access.access_size, access.linked_byte_address,
                          access.address_basis},
    geometry);
  for (auto & mapping : mappings)
  {
    mapping.operation = access.operation;
    mapping.source_access_ordinal = access.source_access_ordinal;
  }
  return mappings;
}

}  // namespace yarda
