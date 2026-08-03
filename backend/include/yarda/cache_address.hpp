#pragma once

#include <cstdint>

namespace yarda
{

/** @brief Hardware parameters used to partition a data-cache address. */
struct CacheGeometry
{
  std::uint64_t line_size = 0;
  std::uint64_t line_count = 0;
  std::uint64_t associativity = 0;
};

/** @brief Cache-line identity represented by its tag and set index. */
struct CacheLineId
{
  std::uint64_t tag = 0;
  std::uint64_t set_index = 0;

  /**
   * @brief Compare cache-line identity without considering byte offset.
   *
   * @param other Cache-line identity to compare.
   * @return True when tag and set index are equal.
   */
  bool operator==(const CacheLineId & other) const
  {
    return tag == other.tag && set_index == other.set_index;
  }
};

/** @brief Virtual address partitioned using one cache geometry. */
struct DecodedCacheAddress
{
  std::uint64_t address = 0;
  std::uint64_t block_number = 0;
  std::uint64_t tag = 0;
  std::uint64_t set_index = 0;
  std::uint64_t line_offset = 0;
};

/**
 * @brief Return the number of cache sets described by a geometry.
 *
 * @param geometry Non-zero, power-of-two cache parameters.
 * @return Number of sets (`line_count / associativity`).
 * @throws std::invalid_argument if the geometry cannot describe a cache.
 */
std::uint64_t cache_set_count(const CacheGeometry & geometry);

/**
 * @brief Partition a virtual address into cache Tag, Index, and Offset.
 *
 * @param address Virtual byte address to decode.
 * @param geometry Non-zero, power-of-two cache parameters.
 * @return Address, block number, tag, set index, and line offset.
 * @throws std::invalid_argument if the geometry cannot describe a cache.
 */
DecodedCacheAddress decode_cache_address(std::uint64_t address,
                                         const CacheGeometry & geometry);

}  // namespace yarda
