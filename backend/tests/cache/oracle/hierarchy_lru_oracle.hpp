#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "yarda/cache/lru_rd_analysis.hpp"
#include "yarda/trace/mapped_trace.hpp"

namespace yarda
{
namespace test
{

/** @brief First level that services one oracle hierarchy reference. */
enum class OracleFirstServiceLevel
{
  L1,
  LLC,
  Memory,
};

/** @brief Bounded-LRU counters for one oracle cache level. */
struct OracleLevelCounts
{
  std::uint64_t lookups = 0;
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t cold_misses = 0;
  std::uint64_t replacement_misses = 0;
};

/** @brief Observable hierarchy decisions for one L1 input reference. */
struct OracleHierarchyEvent
{
  LruAccessOutcome l1_outcome = LruAccessOutcome::ColdMiss;
  std::optional<LruAccessOutcome> llc_outcome;
  OracleFirstServiceLevel first_service = OracleFirstServiceLevel::Memory;
};

/** @brief Explicit hierarchy result for one cold-started task. */
struct OracleTaskHierarchy
{
  std::string task_id;
  std::vector<OracleHierarchyEvent> events;
  OracleLevelCounts l1;
  OracleLevelCounts llc;
  std::uint64_t ehc_l1 = 0;
  std::uint64_t ehc_llc = 0;
  std::uint64_t all_cache_misses = 0;
};

/**
 * @brief Simulate the v1 hierarchy with explicit per-set resident LRU lists.
 *
 * Each invocation creates cold L1, LLC, and ever-seen state. The oracle keeps
 * only bounded resident order and does not compute finite reuse distances.
 *
 * @param l1_trace Ordered task references already mapped at L1 geometry.
 * @param l1_geometry Geometry used by `l1_trace`.
 * @param llc_geometry Equal-line-size geometry for the selected LLC.
 * @return Per-reference first-service decisions and cache-level counters.
 * @throws std::invalid_argument if geometry or a mapping is inconsistent.
 * @throws std::overflow_error if an oracle counter overflows.
 */
OracleTaskHierarchy analyze_with_explicit_lru(
  const MappedTaskTrace & l1_trace, const CacheGeometry & l1_geometry,
  const CacheGeometry & llc_geometry);

}  // namespace test
}  // namespace yarda
