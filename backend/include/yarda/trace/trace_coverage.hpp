#pragma once

#include <cstdint>

namespace yarda
{

/** @brief Coverage within the analyzed roots selected for strict resolution. */
struct TraceCoverage
{
  /** @brief Selected source accesses visited by the strict resolver. */
  std::uint64_t source_accesses = 0;
  /** @brief Source accesses converted to linked byte ranges. */
  std::uint64_t resolved_accesses = 0;
  /** @brief Source accesses rejected before a complete result was produced. */
  std::uint64_t rejected_accesses = 0;
  /** @brief Cache-line rows emitted; zero for geometry-independent results. */
  std::uint64_t emitted_line_references = 0;

  /**
   * @brief Report whether every visited source access was resolved.
   *
   * Successful strict APIs guarantee this condition. Error snapshots can be
   * incomplete and retain the counters accumulated through the rejection.
   * Functions excluded by root-role selection are outside these counters.
   *
   * @return `true` when every visited access resolved without rejection.
   */
  [[nodiscard]] bool complete() const noexcept
  {
    return rejected_accesses == 0 && source_accesses == resolved_accesses;
  }
};

}  // namespace yarda
