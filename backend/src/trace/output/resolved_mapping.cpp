#include "yarda/trace/resolved_mapping.hpp"

#include <stdexcept>

namespace yarda
{

void for_each_cache_line(const ResolvedAccess & access,
                         const CacheGeometry & geometry,
                         const CacheLineSink & sink)
{
  if (!sink)
  {
    throw std::invalid_argument("cache-line sink must not be empty");
  }
  for_each_cache_line(
    CacheLineAddressRange{access.object_id, access.object_byte_offset,
                          access.access_size, access.linked_byte_address,
                          access.address_basis},
    geometry, [&](const CacheLineMapping & line) {
      auto mapping = line;
      mapping.operation = access.operation;
      mapping.source_access_ordinal = access.source_access_ordinal;
      sink(mapping);
    });
}

void for_each_cache_line(const ResolvedAccess & access,
                         const CacheGeometry & geometry,
                         const CacheLineSink & sink,
                         TraceEmissionBudget & budget)
{
  if (!sink)
  {
    throw std::invalid_argument("cache-line sink must not be empty");
  }
  for_each_cache_line(access, geometry, [&](const CacheLineMapping & mapping) {
    budget.consume_line_reference();
    sink(mapping);
  });
}

std::vector<CacheLineMapping> map_cache_lines(const ResolvedAccess & access,
                                              const CacheGeometry & geometry)
{
  std::vector<CacheLineMapping> mappings;
  for_each_cache_line(access, geometry, [&](const CacheLineMapping & mapping) {
    mappings.push_back(mapping);
  });
  return mappings;
}

}  // namespace yarda
