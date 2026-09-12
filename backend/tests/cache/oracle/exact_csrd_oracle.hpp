#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "yarda/cache/line_mapping.hpp"
#include "yarda/cache/lru_rd_analysis.hpp"

namespace yarda
{
namespace test
{

/** @brief Exact CSRD and LRU outcome computed by the naive test oracle. */
struct OracleCsrdObservation
{
  std::optional<std::uint64_t> distance;
  LruAccessOutcome outcome = LruAccessOutcome::ColdMiss;
};

/**
 * @brief Compute full exact CSRD by directly scanning earlier references.
 *
 * This test-only oracle deliberately avoids the production Fenwick and reuse
 * distance analyzers so it can provide an independent expected result.
 *
 * @param accesses Ordered cache-line mappings for one task and cache level.
 * @param geometry Geometry used to produce the mappings.
 * @return One exact observation for every input mapping, in input order.
 * @throws std::invalid_argument if geometry or a mapping is inconsistent.
 * @throws std::overflow_error if a finite distance exceeds `uint64_t`.
 */
std::vector<OracleCsrdObservation>
naive_exact_csrd(const std::vector<CacheLineMapping> & accesses,
                 const CacheGeometry & geometry);

}  // namespace test
}  // namespace yarda
