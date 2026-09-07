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
 * source-to-L1 expansion; modeled_accesses counts L1 line references. EHC
 * counts references first serviced by each cache; all_cache_misses counts
 * references first serviced by Memory. Every ratio uses modeled_accesses as
 * its denominator and is absent when that count is zero (future JSON null).
 * Integer counts are authoritative; ratios and invariants are derived.
 */
struct TaskHierarchySummary
{
  std::string task_id;
  std::uint64_t source_accesses = 0;
  std::uint64_t modeled_accesses = 0;
  CacheLevelSummary l1;
  CacheLevelSummary llc;
  std::uint64_t ehc_l1 = 0;
  std::uint64_t ehc_llc = 0;
  std::uint64_t all_cache_misses = 0;
  std::optional<double> hr_l1;
  std::optional<double> hr_llc;
  std::optional<double> miss_ratio;
  TraceCoverage coverage;
  HierarchyInvariants invariants;
};

}  // namespace yarda
