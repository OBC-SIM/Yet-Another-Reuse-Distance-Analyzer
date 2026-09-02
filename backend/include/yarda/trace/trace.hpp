#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

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
 * expanding beyond 1,000,000 iterations, a loop induction value that
 * overflows, or an unknown node type. This node-level entry point has no
 * module metadata; use block_traces for structured cache-line accesses.
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
 * expanding beyond 1,000,000 iterations, a loop induction value that
 * overflows, or an unknown node type.
 */
std::vector<NamedTrace> block_traces(
  const nlohmann::json & raw, Granularity granularity = Granularity::Element,
  std::size_t cache_line_size = 32);

}  // namespace yarda
