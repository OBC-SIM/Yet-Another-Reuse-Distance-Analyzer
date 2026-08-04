#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "yarda/cache_address.hpp"

namespace yarda
{

/** @brief Store propagation policy configured for one cache level. */
enum class WritePolicy
{
  WriteBack,
  WriteThrough,
};

/** @brief Line replacement policy configured for one cache level. */
enum class Replacement
{
  LRU,
  FIFO,
  MRU,
};

/** @brief Entry cache assigned to one modeled core. */
struct CoreMapping
{
  std::uint32_t id = 0;
  std::string l1;
};

/** @brief Hardware and policy parameters for one named cache level. */
struct CacheConfig
{
  std::string name;
  std::string role;
  std::optional<std::uint32_t> private_to;
  std::uint64_t size_bytes = 0;
  std::uint64_t line_size = 64;
  std::uint64_t associativity = 8;
  Replacement replacement = Replacement::LRU;
  WritePolicy write_policy = WritePolicy::WriteBack;
  bool write_allocate = true;
  std::uint64_t delay_cycles = 0;
  std::string next;
};

/** @brief Terminal memory configuration for a cache hierarchy. */
struct MemoryConfig
{
  std::string name = "Memory";
  std::uint64_t delay_cycles = 120;
};

/** @brief Versioned core-to-cache hierarchy parsed from one YAML file. */
struct HierarchyConfig
{
  std::uint32_t schema_version = 0;
  std::uint32_t num_cores = 0;
  std::vector<CoreMapping> core_mappings;
  std::vector<CacheConfig> caches;
  MemoryConfig memory;
};

/**
 * @brief Find a cache by its stable configuration name.
 *
 * @param config Validated hierarchy to search.
 * @param name Cache name referenced by topology edges.
 * @return Borrowed cache configuration; ownership remains with `config`.
 * @throws std::invalid_argument if no cache has the requested name.
 */
const CacheConfig & find_cache_config(const HierarchyConfig & config,
                                      const std::string & name);

/**
 * @brief Resolve the entry L1 assigned to a modeled core.
 *
 * @param config Validated hierarchy containing every core mapping.
 * @param core_id Core identifier in `[0, num_cores)`.
 * @return Borrowed entry cache; ownership remains with `config`.
 * @throws std::invalid_argument if the core or its mapping is invalid.
 */
const CacheConfig & entry_cache_config(const HierarchyConfig & config,
                                       std::uint32_t core_id);

/**
 * @brief Convert one cache capacity into YARDA address geometry.
 *
 * @param cache Cache with byte capacity, line size, and associativity.
 * @return Geometry accepted by cache address decoding.
 * @throws std::invalid_argument if capacity or geometry is inconsistent.
 */
CacheGeometry make_cache_geometry(const CacheConfig & cache);

/**
 * @brief Validate topology, references, and every cache geometry.
 *
 * @param config Parsed hierarchy to validate without taking ownership.
 * @return Nothing.
 * @throws std::invalid_argument if the hierarchy is not deterministic.
 */
void validate_cache_config(const HierarchyConfig & config);

}  // namespace yarda
