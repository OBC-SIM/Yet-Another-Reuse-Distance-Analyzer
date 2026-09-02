#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "yarda/access_operation.hpp"
#include "yarda/elf/address_model.hpp"
#include "yarda/trace/resolution_error.hpp"
#include "yarda/trace/task_trace.hpp"
#include "yarda/trace/trace_coverage.hpp"

namespace yarda
{

/** @brief Geometry-independent linked address for one source access. */
struct ResolvedAccess
{
  /** @brief Canonical storage-object identifier for the source access. */
  std::string object_id;
  /** @brief Object-relative byte offset where the access begins. */
  std::uint64_t object_byte_offset = 0;
  /** @brief Positive number of consecutive bytes touched by the access. */
  std::uint64_t access_size = 0;
  /** @brief Linked byte address corresponding to the access start. */
  std::uint64_t linked_byte_address = 0;
  /** @brief Address-space interpretation applied to the linked byte address. */
  AddressBasis address_basis = AddressBasis::Absolute;
  /** @brief Memory operation performed by the source access. */
  AccessOperation operation = AccessOperation::Unknown;
  /**
   * @brief Source emission position within the enclosing trace scope.
   *
   * The block API assigns module-wide ordinals. The task API restarts the
   * ordinal at zero for each analyzed root.
   */
  std::uint64_t source_access_ordinal = 0;
};

/** @brief Ordered resolved accesses associated with one reported block. */
struct NamedResolvedTrace
{
  /** @brief Stable report label for the source block. */
  std::string name;
  /** @brief Resolved accesses in source emission order. */
  std::vector<ResolvedAccess> accesses;
};

/** @brief Geometry-independent resolved traces from one LAT module. */
struct ResolvedTraceResult
{
  /** @brief Named resolved traces in deterministic module order. */
  std::vector<NamedResolvedTrace> traces;
  /** @brief Complete source-to-linked-address coverage for this result. */
  TraceCoverage coverage;
};

/** @brief Geometry-independent accesses belonging to one analyzed task. */
using ResolvedTaskTrace = TaskTrace<ResolvedAccess>;

/** @brief Task-isolated resolved traces from one LAT module. */
struct ResolvedTaskTraceResult
{
  /** @brief Analyzed tasks in deterministic module order. */
  std::vector<ResolvedTaskTrace> tasks;
  /** @brief Aggregate coverage across every selected task. */
  TraceCoverage coverage;
  /** @brief Aggregate static call sites omitted as known opaque calls. */
  std::uint64_t excluded_opaque_call_sites = 0;
};

/**
 * @brief Expand LAT accesses and resolve them to linked byte addresses.
 *
 * The result retains source access size, operation, and deterministic emission
 * ordinal without applying cache geometry. Ordinals are module-wide for this
 * API. Every visited access must resolve; unsupported or unresolved accesses
 * fail the operation instead of being omitted.
 *
 * @param raw APE v2 LAT module with canonical object metadata.
 * @param objects Linked global object addresses (borrowed, ownership retained).
 * @return Named geometry-independent access traces in deterministic order.
 * @throws std::invalid_argument for malformed LAT input or call expansion.
 * @throws ResolutionError for unsupported storage or unresolved byte ranges.
 */
ResolvedTraceResult resolved_block_traces(const nlohmann::json & raw,
                                          const ObjectAddressModel & objects);

/**
 * @brief Resolve one ordered access trace for every analyzed task root.
 *
 * Only known callees marked `ape.inline` or `yard.inline` are expanded at
 * their call sites; known non-inline calls are opaque and outside access
 * coverage, while unknown targets are rejected. Opaque static call sites are
 * counted separately for every task. Source ordinals restart at zero for each
 * task, while result coverage aggregates all selected tasks. A call to another
 * analyzed root remains a caller-local opaque site; the callee is also emitted
 * independently as its own task.
 * Empty analyzed roots remain present as empty tasks. Every visited access
 * must resolve.
 *
 * @param raw APE v2 LAT module with canonical object metadata.
 * @param objects Linked global object addresses (borrowed, ownership retained).
 * @return Task-isolated resolved traces in deterministic module order.
 * @throws std::invalid_argument if no explicitly analyzed root exists, or for
 * malformed LAT input, ambiguous or overlapping function roles, unknown call
 * targets, or invalid inline-call expansion.
 * @throws ResolutionError for unsupported storage or unresolved byte ranges.
 */
ResolvedTaskTraceResult resolved_task_traces(
  const nlohmann::json & raw, const ObjectAddressModel & objects);

}  // namespace yarda
