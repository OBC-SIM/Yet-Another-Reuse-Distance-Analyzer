#include "yarda/cache/address.hpp"

#include <stdexcept>

namespace yarda
{
namespace
{

bool is_power_of_two(std::uint64_t value)
{
  return value != 0 && (value & (value - 1)) == 0;
}

void validate(const CacheGeometry & geometry)
{
  if (!is_power_of_two(geometry.line_size) ||
      !is_power_of_two(geometry.line_count) ||
      !is_power_of_two(geometry.associativity))
  {
    throw std::invalid_argument(
      "cache line size, line count, and associativity must be powers of two");
  }
  if (geometry.associativity > geometry.line_count ||
      geometry.line_count % geometry.associativity != 0)
  {
    throw std::invalid_argument(
      "cache associativity must divide the cache line count");
  }
}

}  // namespace

std::uint64_t cache_set_count(const CacheGeometry & geometry)
{
  validate(geometry);
  return geometry.line_count / geometry.associativity;
}

DecodedCacheAddress decode_cache_address(std::uint64_t address,
                                         const CacheGeometry & geometry)
{
  const auto set_count = cache_set_count(geometry);
  const auto block_number = address / geometry.line_size;
  return {
    address,
    block_number,
    block_number / set_count,
    block_number % set_count,
    address % geometry.line_size,
  };
}

}  // namespace yarda
