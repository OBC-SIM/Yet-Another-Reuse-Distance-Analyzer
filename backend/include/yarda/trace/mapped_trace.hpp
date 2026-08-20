#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "yarda/cache/line_mapping.hpp"

namespace yarda
{

/** @brief Ordered mapped accesses associated with one reported block. */
struct NamedMappedTrace
{
  std::string name;
  std::vector<CacheLineMapping> accesses;
};

/** @brief Exact typed traces and unique mapping rows. */
struct MappedTraceResult
{
  std::vector<NamedMappedTrace> traces;
  CacheLineMappingTable mappings;
};

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
 * @brief Concatenate mapped blocks without resetting program access order.
 *
 * @param traces Mapped blocks in execution order.
 * @return Cache-line mappings in one program-wide access sequence.
 */
std::vector<CacheLineMapping>
flatten_mapped_traces(const std::vector<NamedMappedTrace> & traces);

/**
 * @brief Generate exact traces with linked global cache-address mapping.
 *
 * @param raw APE v2 LAT module containing canonical object IDs.
 * @param geometry Cache geometry used to decode Tag, Index, and Offset.
 * @param objects Linked global object addresses (borrowed, ownership retained).
 * @return Named cache-line traces and deterministic mapping-table rows.
 * @throws std::invalid_argument for unresolved or unsupported global accesses.
 */
MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects);

}  // namespace yarda
