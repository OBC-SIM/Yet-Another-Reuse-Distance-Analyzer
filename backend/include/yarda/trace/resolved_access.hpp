#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "yarda/access_operation.hpp"
#include "yarda/elf/address_model.hpp"
#include "yarda/trace/resolution_error.hpp"
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
  /** @brief Module-wide source emission position of this access. */
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

}  // namespace yarda
