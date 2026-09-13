#pragma once

#include <nlohmann/json.hpp>

#include "yarda/cache/artifact_identity.hpp"
#include "yarda/cache/streaming_hierarchy.hpp"

namespace yarda
{

/**
 * @brief Own provenance and a selected model snapshot for a complete result.
 *
 * The caller must bind these values to the same raw inputs used by successful
 * ET_EXEC/absolute-address analysis. Schema versions must be LAT 2 and cache 1;
 * ELF address size is in bytes. Paths and runtime observations are excluded.
 */
struct HierarchyResultMetadata
{
  AnalysisIdentityInput identity;
  AnalysisHierarchy hierarchy;
  std::uint32_t lat_schema_version = 0;
  std::uint32_t cache_schema_version = 0;
  std::uint8_t elf_address_size = 0;
  std::uint16_t elf_machine = 0;
};

/**
 * @brief Serialize complete semantic summaries without reading external state.
 * @param metadata Borrowed input identity and effective selected hierarchy.
 * @param result Borrowed successful streaming result, with at least one task.
 * @return Ordered schema-v1 JSON; use dump() directly to preserve key order.
 * @throws std::invalid_argument for inconsistent metadata or summaries.
 * @throws std::logic_error for violated analyzer conservation contracts.
 * @throws std::overflow_error for count or byte-capacity overflow.
 * @note Ratios are emitted unchanged after validation with the analyzer's
 * shared finalizer. No trace is reconstructed or analyzed again.
 * @note Non-identity text is retained without UTF-8 validation. Supply valid
 * UTF-8 for publication; the caller's subsequent dump() can otherwise throw
 * nlohmann::json::type_error. Complete dumping before opening an output file.
 */
nlohmann::ordered_json
hierarchy_result_json(const HierarchyResultMetadata & metadata,
                      const StreamingHierarchyResult & result);

} // namespace yarda
