#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "yarda/cache/address.hpp"
#include "yarda/trace/mapped_trace.hpp"

namespace yarda
{
namespace test
{
namespace support
{

/** @brief Inputs for one deterministic oracle differential trace. */
struct SeededOracleTraceSpec
{
  /** @brief Seed passed to the standard Mersenne Twister engine. */
  std::uint32_t seed = 0;
  /** @brief Total number of generated cache-line references. */
  std::size_t reference_count = 0;
  /** @brief Number of cache blocks available to random references. */
  std::uint64_t block_domain = 0;
  /** @brief Geometry used to map the generated task trace. */
  CacheGeometry l1_geometry;
  /** @brief Equal-line-size geometry used for LLC comparison. */
  CacheGeometry llc_geometry;
};

/**
 * @brief Build a reproducible mixed-offset and mixed-operation trace.
 *
 * A deterministic witness prefix covers multi-offset identity, eviction
 * history, and store recency before the seeded suffix broadens combinations.
 *
 * @param spec Valid geometries, a positive block domain, and at least thirteen
 * references.
 * @return L1-mapped task trace whose task ID identifies the seed.
 * @throws std::invalid_argument if the block domain is zero.
 */
MappedTaskTrace make_seeded_oracle_trace(const SeededOracleTraceSpec & spec);

/**
 * @brief Render complete reproduction context for a differential failure.
 *
 * @param spec Generator inputs used for the trace.
 * @param trace Generated task trace to render.
 * @return Seed, geometries, length, and ordered block/offset/operation tokens.
 */
std::string describe_seeded_oracle_trace(const SeededOracleTraceSpec & spec,
                                         const MappedTaskTrace & trace);

}  // namespace support
}  // namespace test
}  // namespace yarda
