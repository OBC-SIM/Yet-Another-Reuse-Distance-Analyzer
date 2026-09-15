#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "yarda/trace/work_limits.hpp"

namespace yarda
{

/**
 * @brief Identity used when materializing memory references.
 */
enum class Granularity
{
  Element,
  CacheLine,
};

/** @brief Ordered string accesses associated with one reported block. */
struct NamedTrace
{
  std::string name;
  std::vector<std::string> accesses;
};

/**
 * @brief Expand one LAT node using its actual loop bounds.
 *
 * @param node LAT node after call expansion.
 * @param granularity Element or cache-line reference identity.
 * @param cache_line_size Cache-line size in bytes.
 * @return Ordered reference keys.
 * @throws std::invalid_argument if CacheLine granularity is requested for a
 * structured field path, or if the node holds a zero loop step, a loop
 * expanding beyond 1,000,000 iterations, cumulative loop work above
 * 1,000,000 iterations, a loop induction value that overflows, or an unknown
 * node type. This node-level entry point has no module metadata; use
 * block_traces for structured cache-line accesses.
 */
std::vector<std::string> unroll_node_actual(
  const nlohmann::json & node, Granularity granularity = Granularity::Element,
  std::size_t cache_line_size = 32);

/**
 * @brief Generate ordered block traces for every analyzed function.
 *
 * @param raw Legacy or APE v2 LAT module.
 * @param granularity Element or cache-line reference identity.
 * @param cache_line_size Cache-line size in bytes.
 * @return Named loop and flat traces in module order.
 * @throws std::invalid_argument for malformed or unsupported structured
 * layout metadata, including pointer-backed structured objects; for a
 * malformed module schema or call expansion; and for a zero loop step, a loop
 * expanding beyond 1,000,000 iterations, cumulative loop work above
 * 1,000,000 iterations, more than 100,000 call-expansion node visits, an inline
 * call depth above 256, a loop induction value that overflows, or an unknown
 * node type.
 */
std::vector<NamedTrace> block_traces(
  const nlohmann::json & raw, Granularity granularity = Granularity::Element,
  std::size_t cache_line_size = 32);

/**
 * @brief Generate block traces with explicit module-wide loop allowances.
 *
 * Uses the same layout, call-expansion and ordering contract as block_traces
 * with default limits. All functions share one cumulative loop budget.
 *
 * @param raw Borrowed legacy or APE v2 LAT module, unchanged by traversal.
 * @param granularity Element or cache-line reference identity.
 * @param cache_line_size Cache-line size in bytes.
 * @param loop_limits Borrowed inclusive allowances, copied for this invocation.
 * @return Named loop and flat traces in module order.
 * @throws std::invalid_argument for unsupported input or exhausted loop work.
 * Structural node/depth limits remain unchanged.
 */
std::vector<NamedTrace> block_traces(
  const nlohmann::json & raw, Granularity granularity,
  std::size_t cache_line_size, const LoopWorkLimits & loop_limits);

}  // namespace yarda
