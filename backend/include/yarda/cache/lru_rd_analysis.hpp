#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "yarda/cache/address.hpp"
#include "yarda/cache/line_mapping.hpp"

namespace yarda
{

/** @brief LRU outcome assigned to one cache-line access. */
enum class LruAccessOutcome
{
  Hit,
  ColdMiss,
  ReplacementMiss,
};

/** @brief LRU decision aligned with one input access. */
struct LruAccessResult
{
  LruAccessOutcome outcome = LruAccessOutcome::ColdMiss;
  std::optional<std::size_t> reuse_distance;
};

/** @brief Aggregate reuse and miss counts for one cache set. */
struct LruSetResult
{
  std::map<std::size_t, std::uint64_t> histogram;
  std::uint64_t hits = 0;
  std::uint64_t cold_misses = 0;
  std::uint64_t replacement_misses = 0;
};

/** @brief Set-aware LRU reuse-distance result for an ordered trace. */
struct LruRdAnalysis
{
  std::vector<LruAccessResult> accesses;
  std::map<std::size_t, std::uint64_t> histogram;
  std::map<std::uint64_t, LruSetResult> sets;
  std::uint64_t hits = 0;
  std::uint64_t cold_misses = 0;
  std::uint64_t replacement_misses = 0;
};

/**
 * @brief Analyze exact LRU reuse distance independently within each set.
 *
 * The returned access decisions retain input order. A non-cold access hits
 * exactly when its set-local reuse distance is below the associativity.
 *
 * @param accesses Ordered mappings decoded with `geometry`.
 * @param geometry Cache geometry defining set count and associativity.
 * @return Per-access decisions plus set-level and aggregate counts.
 * @throws std::invalid_argument if geometry or a set index is invalid.
 */
LruRdAnalysis analyze_lru_reuse(const std::vector<CacheLineMapping> & accesses,
                                const CacheGeometry & geometry);

}  // namespace yarda
