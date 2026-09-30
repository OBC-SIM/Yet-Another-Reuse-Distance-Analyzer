#pragma once

#include <cstdint>

namespace yarda
{

/** @brief Cumulative emission limits for one streaming module execution. */
struct TraceEmissionLimits
{
  /** @brief Maximum source accesses visited across all tasks. */
  std::uint64_t emitted_source_accesses = 1'000'000;
  /** @brief Maximum line references delivered across all source accesses. */
  std::uint64_t emitted_line_references = 10'000'000;
};

/**
 * @brief Share emission limits between source production and line mapping.
 *
 * Create one budget per module and pass the same instance to its producer and
 * budgeted line mapper. Task boundaries do not reset it. A reservation occurs
 * before resolution or delivery; discard the budget and partial consumer state
 * if any operation fails. Structural expansion limits are enforced separately.
 */
class TraceEmissionBudget
{
public:
  /**
   * @brief Start a module with no emissions charged.
   * @param limits Inclusive source and line limits; zero prohibits emission.
   */
  explicit TraceEmissionBudget(TraceEmissionLimits limits = {});

  /**
   * @brief Reserve one source access before resolution.
   * @return Nothing.
   * @throws std::invalid_argument if the cumulative source limit is exhausted.
   */
  void consume_source_access();

  /**
   * @brief Reserve one line reference before delivery to its consumer.
   * @return Nothing.
   * @throws std::invalid_argument if the cumulative line limit is exhausted.
   */
  void consume_line_reference();

private:
  TraceEmissionLimits limits_;
  std::uint64_t source_accesses_ = 0;
  std::uint64_t line_references_ = 0;
};

}  // namespace yarda
