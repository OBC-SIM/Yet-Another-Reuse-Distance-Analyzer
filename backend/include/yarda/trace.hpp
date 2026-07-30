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

/**
 * @brief Ordered accesses associated with one reported function block.
 */
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
 */
std::vector<std::string> unroll_node_actual(
  const nlohmann::json & node, Granularity granularity = Granularity::Element,
  std::size_t cache_line_size = 32);

/**
 * @brief Expand nested loops with per-depth simulation bounds.
 *
 * @param node LAT node after call expansion.
 * @param simulation_bounds Maximum iterations for each loop depth.
 * @return Ordered element-reference keys.
 */
std::vector<std::string>
unroll_node_sample(const nlohmann::json & node,
                   const std::vector<std::size_t> & simulation_bounds);

/**
 * @brief Generate ordered block traces for every analyzed function.
 *
 * @param raw Legacy or APE v2 LAT module.
 * @param granularity Element or cache-line reference identity.
 * @param cache_line_size Cache-line size in bytes.
 * @return Named loop and flat traces in module order.
 */
std::vector<NamedTrace> block_traces(
  const nlohmann::json & raw, Granularity granularity = Granularity::Element,
  std::size_t cache_line_size = 32);

}  // namespace yarda
