#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "yarda/cache/line_mapping.hpp"
#include "yarda/trace/resolution_error.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/task_trace.hpp"
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

/** @brief Cache-line references belonging to one analyzed task. */
using MappedTaskTrace = TaskTrace<CacheLineMapping>;

/** @brief Task-isolated mapped traces and unique address rows. */
struct MappedTaskTraceResult
{
  /** @brief Analyzed tasks in deterministic module order. */
  std::vector<MappedTaskTrace> tasks;
  /** @brief Deduplicated address rows shared across the module. */
  CacheLineMappingTable mappings;
  /** @brief Aggregate coverage across every selected task. */
  TraceCoverage coverage;
  /** @brief Aggregate static call sites omitted as known opaque calls. */
  std::uint64_t excluded_opaque_call_sites = 0;
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
 * call expansion, or an expansion resource limit violation.
 * @throws ResolutionError for unsupported storage or unresolved byte ranges.
 */
MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects);

/**
 * @brief Map already resolved task accesses at one cache geometry.
 *
 * Task boundaries and task-local source ordinals remain unchanged. Every
 * source access produces one or more ordered cache-line references.
 *
 * @param resolved Geometry-independent task traces.
 * @param geometry Cache geometry used to decode Tag, Index, and Offset.
 * @return Task-isolated mapped traces and deterministic address-only rows.
 * @throws std::invalid_argument for invalid geometry, incomplete coverage,
 * empty or duplicate task identities, or invalid resolved ranges.
 * @throws std::overflow_error if a resolved range overflows.
 */
MappedTaskTraceResult map_resolved_task_traces(
  const ResolvedTaskTraceResult & resolved, const CacheGeometry & geometry);

/**
 * @brief Resolve and map every analyzed task without merging task boundaries.
 *
 * @param raw APE v2 LAT module containing canonical object IDs.
 * @param geometry Cache geometry used to decode Tag, Index, and Offset.
 * @param objects Linked global object addresses (borrowed, ownership retained).
 * @return Task-isolated cache-line traces with complete coverage.
 * @throws std::invalid_argument for invalid geometry or any condition rejected
 * by resolved_task_traces, including a missing analyzed root, overlapping
 * roles, ambiguous identities, unknown targets, malformed LAT input, or an
 * expansion resource limit violation.
 * @throws ResolutionError for unsupported storage or unresolved byte ranges.
 */
MappedTaskTraceResult mapped_task_traces(const nlohmann::json & raw,
                                         const CacheGeometry & geometry,
                                         const ObjectAddressModel & objects);

}  // namespace yarda
