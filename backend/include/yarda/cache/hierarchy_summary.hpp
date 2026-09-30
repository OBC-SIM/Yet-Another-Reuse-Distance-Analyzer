#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "yarda/trace/trace_coverage.hpp"

namespace yarda
{

/**
 * @brief Count one cold-started cache-level input stream.
 *
 * The accounting unit is a cache-line reference. Finite distances, including
 * those above associativity, remain separate histogram keys. Unique lines
 * count linked blocks independently of source object identity.
 */
struct CacheLevelSummary
{
  std::uint64_t lookups = 0;
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t cold_misses = 0;
  std::uint64_t replacement_misses = 0;
  std::uint64_t unique_lines = 0;
  std::map<std::uint64_t, std::uint64_t> csrd_histogram;
};

/**
 * @brief Record checked conservation for a complete hierarchy task.
 *
 * Successful analysis returns all five flags true. A failed check throws
 * before a result is returned; default flags do not assert validity.
 */
struct HierarchyInvariants
{
  bool level_conservation_l1 = false;
  bool level_conservation_llc = false;
  bool llc_input_matches_l1_misses = false;
  bool first_service_conservation = false;
  bool all_passed = false;
};

/**
 * @brief Own semantic counts for one independently cold-started task.
 *
 * No per-reference payload is retained. Source counts and coverage describe
 * source-to-L1 expansion; modeled_accesses counts L1 cache-line references.
 * Level-Wise First-Hit Counts count references first hitting L1 or LLC;
 * all_cache_misses is the All-Cache Miss Count, serviced by Memory.
 * The Cache-Level Profile consists of l1_first_hit_ratio, llc_first_hit_ratio
 * and all_cache_miss_ratio, in that order. Every ratio uses modeled_accesses
 * as its denominator and is absent (serialized as JSON null) when it is zero.
 * Integer counts are authoritative; ratios and invariants are derived.
 */
struct TaskHierarchySummary
{
  std::string task_id;
  std::uint64_t source_accesses = 0;
  std::uint64_t modeled_accesses = 0;
  CacheLevelSummary l1;
  CacheLevelSummary llc;
  std::uint64_t l1_first_hit_count = 0;
  std::uint64_t llc_first_hit_count = 0;
  std::uint64_t all_cache_misses = 0;
  std::optional<double> l1_first_hit_ratio;
  std::optional<double> llc_first_hit_ratio;
  std::optional<double> all_cache_miss_ratio;
  TraceCoverage coverage;
  HierarchyInvariants invariants;
};

}  // namespace yarda
