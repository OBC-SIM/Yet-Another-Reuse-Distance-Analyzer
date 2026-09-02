#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>

#include "yarda/cache/address.hpp"
#include "yarda/elf/data_regions.hpp"
#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace yarda
{

/** @brief Input identities and geometry attached to one task-mapping report. */
struct TaskMappingReportMetadata
{
  /** @brief LAT path supplied to the analyzer. */
  std::string lat_path;
  /** @brief ELF path supplied to the analyzer. */
  std::string elf_path;
  /** @brief Cache-configuration path supplied to the analyzer. */
  std::string cache_path;
  /** @brief Parsed ELF image type; only `Executable` is reportable. */
  std::optional<ElfImageType> elf_image_type;
  /** @brief Address width declared by the ELF image. */
  std::uint8_t elf_address_size = 0;
  /** @brief Machine identifier declared by the ELF image. */
  std::uint16_t elf_machine = 0;
  /** @brief Cache selected for address decoding. */
  std::string cache_name;
  /** @brief Geometry selected for address decoding. */
  CacheGeometry geometry;
};

/**
 * @brief Serialize resolved and cache-mapped task traces.
 *
 * The report accepts only absolute linked addresses and uses the stable
 * `linked_absolute` address-basis label. Resolved and mapped task identities
 * must correspond exactly.
 *
 * @param metadata Input identities and selected cache geometry.
 * @param resolved Geometry-independent task accesses.
 * @param mapped Cache-line mappings derived from `resolved`.
 * @return Versioned deterministic JSON report.
 * @throws std::invalid_argument if metadata or task results are inconsistent,
 * if the image is not `ET_EXEC`, or if an access does not use an absolute
 * linked address.
 * @pre `mapped` is the unmodified result of mapping `resolved` with
 * `metadata.geometry`.
 */
nlohmann::json task_mapping_json(const TaskMappingReportMetadata & metadata,
                                 const ResolvedTaskTraceResult & resolved,
                                 const MappedTaskTraceResult & mapped);

}  // namespace yarda
