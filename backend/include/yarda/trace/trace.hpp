#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "yarda/trace/mapped_trace.hpp"

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
 */
std::vector<std::string> unroll_node_actual(
  const nlohmann::json & node, Granularity granularity = Granularity::Element,
  std::size_t cache_line_size = 32);

/**
 * @brief Expand one LAT node with linked global cache-address mapping.
 *
 * @param node LAT node after call expansion.
 * @param geometry Cache geometry used to decode Tag, Index, and Offset.
 * @param objects Linked global object addresses (borrowed, ownership retained).
 * @return Ordered typed cache-line mappings.
 * @throws std::invalid_argument for unresolved or unsupported global accesses.
 */
std::vector<CacheLineMapping>
unroll_node_actual(const nlohmann::json & node, const CacheGeometry & geometry,
                   const ObjectAddressModel & objects);

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
