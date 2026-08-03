#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "yarda/cache_address.hpp"
#include "yarda/object_addresses.hpp"

namespace yarda
{

/** @brief One object location decoded into the target cache address fields. */
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
 * @brief Map one object-relative access to cache Tag, Index, and Offset.
 *
 * @param object_id Canonical LAT storage object ID.
 * @param object_byte_offset Byte offset from the linked object base.
 * @param access_size Number of bytes touched by the access.
 * @param objects Linked object-address model (borrowed, ownership retained).
 * @param geometry Cache geometry used to partition the resulting address.
 * @return Mapping row containing the reconstructed and decoded address.
 * @throws std::invalid_argument for missing objects or out-of-bounds accesses.
 * @throws std::overflow_error if address reconstruction overflows.
 */
CacheLineMapping map_cache_line(const std::string & object_id,
                                std::uint64_t object_byte_offset,
                                std::uint64_t access_size,
                                const ObjectAddressModel & objects,
                                const CacheGeometry & geometry);

}  // namespace yarda
