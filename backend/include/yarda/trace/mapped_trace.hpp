#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "yarda/cache/line_mapping.hpp"
#include "yarda/trace/resolution_error.hpp"
#include "yarda/trace/trace_coverage.hpp"

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
  /** @brief Complete source-to-cache-line coverage for this result. */
  TraceCoverage coverage;
};

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
 * module-wide across functions. Every visited access must resolve; unsupported
 * or unresolved accesses fail the operation instead of being omitted.
 * @throws std::invalid_argument for invalid geometry, malformed LAT input,
 * or call expansion.
 * @throws ResolutionError for unsupported storage or unresolved byte ranges.
 */
MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects);

}  // namespace yarda
