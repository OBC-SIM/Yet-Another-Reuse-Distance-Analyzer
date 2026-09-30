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
 * ET_EXEC/absolute-address analysis. Schema versions must be MAP 2 and cache 1;
 * ELF address size is in bytes. Paths and runtime observations are excluded.
 */
struct HierarchyResultMetadata
{
  AnalysisIdentityInput identity;
  AnalysisHierarchy hierarchy;
  std::uint32_t map_schema_version = 0;
  std::uint32_t cache_schema_version = 0;
  std::uint8_t elf_address_size = 0;
  std::uint16_t elf_machine = 0;
};

/** @brief Own both borrowed callback arguments when retaining an event. */
struct HierarchyEventRecord
{
  std::string task_id;
  HierarchyAccessEvent event;
};

/**
 * @brief Describe an enabled event sink after complete successful analysis.
 *
 * total_line_references comes from result.coverage.emitted_line_references.
 * delivery comes from the same result. Discard records and metadata on any
 * analysis failure, including events from previously completed tasks.
 * Matching delivery counts alone cannot establish that records belong to
 * that successful invocation; the caller owns their lifetime and provenance.
 */
struct HierarchyEventMetadata
{
  std::string analysis_id;
  std::uint64_t event_limit = 0;
  std::uint64_t total_line_references = 0;
  HierarchyEventDelivery delivery;
};

/**
 * @brief Serialize complete semantic summaries without reading external state.
 * @param metadata Borrowed input identity and effective selected hierarchy.
 * @param result Borrowed successful streaming result, with at least one task.
 * @return Ordered RESULT schema-v2 JSON; dump() preserves glossary key order.
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

/**
 * @brief Serialize the retained prefix of one enabled diagnostic stream.
 * @param metadata Borrowed delivery totals from the successful invocation.
 * @param events Borrowed owned records in task/source/span order.
 * @return Ordered schema-v1 JSON with explicit nullable cache observations.
 * @throws std::invalid_argument for inconsistent delivery, order or events.
 * @note This function performs no I/O; callers create no event artifact when
 * diagnostics are disabled. It never revisits the source trace.
 * @note Event text is retained without UTF-8 validation. Supply valid UTF-8
 * for publication; the caller's subsequent dump() can otherwise throw
 * nlohmann::json::type_error. Complete dumping before opening an output file.
 */
nlohmann::ordered_json
hierarchy_events_json(const HierarchyEventMetadata & metadata,
                      const std::vector<HierarchyEventRecord> & events);

} // namespace yarda
