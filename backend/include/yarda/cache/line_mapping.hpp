#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "yarda/access_operation.hpp"
#include "yarda/cache/address.hpp"
#include "yarda/elf/address_model.hpp"

namespace yarda
{

/** @brief First touched byte in one cache line of an object access. */
struct CacheLineMapping
{
  /** @brief Canonical storage-object identifier for this mapping row. */
  std::string object_id;
  /** @brief Object-relative offset of this row's first byte. */
  std::uint64_t object_byte_offset = 0;
  /** @brief Address-space interpretation applied to linked byte addresses. */
  AddressBasis address_basis = AddressBasis::Absolute;
  /** @brief Cache tag, set, and line offset decoded for this row. */
  DecodedCacheAddress decoded;
  /** @brief Object-relative offset where the source access begins. */
  std::uint64_t source_object_byte_offset = 0;
  /** @brief Total byte count of the source access. */
  std::uint64_t source_access_size = 0;
  /** @brief Linked byte address where the source access begins. */
  std::uint64_t source_linked_byte_address = 0;
  /** @brief Memory operation performed by the source access. */
  AccessOperation operation = AccessOperation::Unknown;
  /** @brief Stable source ordinal shared by every row from one access. */
  std::uint64_t source_access_ordinal = 0;
  /** @brief Zero-based position within the source access's cache-line span. */
  std::uint64_t line_span_ordinal = 0;
};

/** @brief Address-only row safe for deduplicated mapping tables. */
struct CacheLineAddressMapping
{
  /** @brief Canonical storage-object identifier for this address row. */
  std::string object_id;
  /** @brief Object-relative offset of this row's first byte. */
  std::uint64_t object_byte_offset = 0;
  /** @brief Address-space interpretation applied to the decoded address. */
  AddressBasis address_basis = AddressBasis::Absolute;
  /** @brief Cache tag, set, and line offset decoded for this row. */
  DecodedCacheAddress decoded;
};

/** @brief Linked byte range ready for cache-geometry decoding. */
struct CacheLineAddressRange
{
  /** @brief Canonical storage-object identifier for the source range. */
  std::string object_id;
  /** @brief Object-relative byte offset where the source range begins. */
  std::uint64_t object_byte_offset = 0;
  /** @brief Positive number of consecutive bytes in the source range. */
  std::uint64_t access_size = 0;
  /** @brief Linked byte address corresponding to the source range start. */
  std::uint64_t linked_byte_address = 0;
  /** @brief Address-space interpretation applied to the linked byte address. */
  AddressBasis address_basis = AddressBasis::Absolute;
};

/** @brief Deterministic address rows keyed by object ID and byte offset. */
using CacheLineMappingTable =
  std::map<std::pair<std::string, std::uint64_t>, CacheLineAddressMapping>;

/**
 * @brief Decode every cache line touched by one linked byte range.
 *
 * This overload needs no object-address model and can remap the same linked
 * range at multiple cache geometries.
 *
 * @param range Linked source byte range.
 * @param geometry Cache geometry used to partition the linked address.
 * @return Ordered mapping rows, one for each touched cache line.
 * @throws std::invalid_argument for invalid geometry or source range.
 * @throws std::overflow_error if the source range overflows.
 */
std::vector<CacheLineMapping> map_cache_lines(
  const CacheLineAddressRange & range, const CacheGeometry & geometry);

}  // namespace yarda
