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
  /** @brief Stable report label for the source block. */
  std::string name;
  /** @brief Cache-line mappings in source access and line-span order. */
  std::vector<CacheLineMapping> accesses;
};

/** @brief Exact typed traces and unique mapping rows. */
struct MappedTraceResult
{
  /** @brief Named mapped traces in deterministic module order. */
  std::vector<NamedMappedTrace> traces;
  /** @brief Deduplicated address rows keyed by object ID and byte offset. */
  CacheLineMappingTable mappings;
};

/**
 * @brief Expand one LAT node with linked global cache-address mapping.
 *
 * @param node LAT node after call expansion.
 * @param geometry Cache geometry used to decode Tag, Index, and Offset.
 * @param objects Linked global object addresses (borrowed, ownership retained).
 * @return Ordered typed cache-line mappings retaining resolved provenance.
 * Source access ordinals start at zero for this node and count omitted scalar
 * and non-global accesses.
 * @throws std::invalid_argument for unresolved global accesses or structured
 * field paths. This direct-node overload has no module metadata; use
 * mapped_block_traces for structured accesses.
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
 * @return Named cache-line traces retaining resolved provenance and
 * deterministic address-only mapping-table rows. Source access ordinals are
 * module-wide across functions and count scalar or non-global accesses omitted
 * from the returned mapped traces. Scalar accesses retain the legacy behavior
 * of being omitted, including when their layout metadata is absent.
 * @throws std::invalid_argument for unresolved global accesses or malformed
 * or unsupported structured metadata, including pointer-backed objects.
 */
MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects);

}  // namespace yarda
