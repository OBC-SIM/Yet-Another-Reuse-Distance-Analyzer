#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#include "yarda/cache/lru_rd_analysis.hpp"

namespace yarda
{
namespace detail
{
class RecencyIndex;
struct ExactCsrdTestAccess;
}  // namespace detail

/** @brief Report one full exact cache-set distance; cold has no distance. */
struct CsrdObservation
{
  LruAccessOutcome outcome = LruAccessOutcome::ColdMiss;
  std::optional<std::uint64_t> distance;
};

/**
 * @brief Own counts for a cold-started cache-level stream without its trace.
 *
 * The histogram retains every finite distance, including replacement misses.
 * Unique lines include all historical linked blocks, including evicted ones.
 */
struct ExactCsrdSummary
{
  std::uint64_t lookups = 0;
  std::uint64_t hits = 0;
  std::uint64_t cold_misses = 0;
  std::uint64_t replacement_misses = 0;
  std::uint64_t unique_lines = 0;
  std::map<std::uint64_t, std::uint64_t> histogram;
};

/**
 * @brief Analyze one task and cache level incrementally with full exact CSRD.
 *
 * Each instance starts cold and owns its history. Retained state is O(V) for
 * V historical distinct blocks; rank/update costs O(log(V + 1)) amortized.
 * Create a new instance for each task. Discard the instance if observe throws.
 */
class ExactCsrdAnalyzer
{
public:
  /**
   * @brief Start an independent cache-level history.
   * @param geometry Valid power-of-two cache geometry, copied into the
   * instance.
   * @throws std::invalid_argument if the geometry is invalid.
   */
  explicit ExactCsrdAnalyzer(CacheGeometry geometry);

  /** @brief Release the owned history and invalidate borrowed summary views. */
  ~ExactCsrdAnalyzer();

  ExactCsrdAnalyzer(const ExactCsrdAnalyzer &) = delete;
  ExactCsrdAnalyzer & operator=(const ExactCsrdAnalyzer &) = delete;

  /**
   * @brief Observe one line reference without retaining the input address.
   *
   * Byte offsets do not distinguish lines. Only same-set distinct blocks
   * contribute to distance; a finite distance below associativity is a hit.
   *
   * @param address Borrowed address decoded with this instance's geometry.
   * @return Outcome and exact distance, absent only for a cold miss.
   * @throws std::invalid_argument if any decoded address field is inconsistent.
   * @throws std::overflow_error if counts, slots or storage sizes overflow.
   */
  CsrdObservation observe(const DecodedCacheAddress & address);

  /**
   * @brief Borrow the aggregate of all successfully observed references.
   * @pre No observation on this instance has failed.
   * @return Read-only view, updated by observe and valid until destruction.
   */
  const ExactCsrdSummary & summary() const noexcept;

private:
  friend struct detail::ExactCsrdTestAccess;
  CacheGeometry geometry_;
  std::uint64_t set_count_ = 0;
  ExactCsrdSummary summary_;
  std::map<std::uint64_t, std::unique_ptr<detail::RecencyIndex>> sets_;
};

}  // namespace yarda
