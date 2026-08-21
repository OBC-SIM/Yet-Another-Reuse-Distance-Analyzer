#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "yarda/cache/address.hpp"
#include "yarda/elf/address_model.hpp"

namespace yarda
{

/** @brief First touched byte in one cache line of an object access. */
struct CacheLineMapping
{
  std::string object_id;
  std::uint64_t object_byte_offset = 0;
  AddressBasis address_basis = AddressBasis::Absolute;
  DecodedCacheAddress decoded;
};

/** @brief Deterministic rows keyed by canonical object ID and byte offset. */
using CacheLineMappingTable =
  std::map<std::pair<std::string, std::uint64_t>, CacheLineMapping>;

/**
 * @brief Map every cache line touched by one object-relative access.
 *
 * The first row starts at `object_byte_offset`. Each subsequent row starts at
 * the first byte touched in the next cache line.
 *
 * @param object_id Canonical LAT storage object ID.
 * @param object_byte_offset Byte offset from the linked object base.
 * @param access_size Number of bytes touched by the access.
 * @param objects Linked object-address model (borrowed, ownership retained).
 * @param geometry Cache geometry used to partition the resulting address.
 * @return Ordered mapping rows, one for each touched cache line.
 * @throws std::invalid_argument for invalid geometry, missing objects, or
 * out-of-bounds accesses.
 * @throws std::overflow_error if address reconstruction overflows.
 */
std::vector<CacheLineMapping>
map_cache_lines(const std::string & object_id,
                std::uint64_t object_byte_offset, std::uint64_t access_size,
                const ObjectAddressModel & objects,
                const CacheGeometry & geometry);

}  // namespace yarda
