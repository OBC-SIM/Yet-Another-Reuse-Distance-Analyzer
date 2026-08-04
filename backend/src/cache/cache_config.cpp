#include "yarda/cache/cache_config.hpp"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace yarda
{

const CacheConfig & find_cache_config(const HierarchyConfig & config,
                                      const std::string & name)
{
  for (const auto & cache : config.caches)
  {
    if (cache.name == name)
    {
      return cache;
    }
  }
  throw std::invalid_argument("unknown cache: " + name);
}

const CacheConfig & entry_cache_config(const HierarchyConfig & config,
                                       std::uint32_t core_id)
{
  if (core_id >= config.num_cores)
  {
    throw std::invalid_argument("core id is outside configured range");
  }
  for (const auto & mapping : config.core_mappings)
  {
    if (mapping.id == core_id)
    {
      return find_cache_config(config, mapping.l1);
    }
  }
  throw std::invalid_argument("core has no entry cache mapping: " +
                              std::to_string(core_id));
}

CacheGeometry make_cache_geometry(const CacheConfig & cache)
{
  if (cache.size_bytes == 0 || cache.line_size == 0 || cache.associativity == 0)
  {
    throw std::invalid_argument("cache geometry values must be positive: " +
                                cache.name);
  }
  if (cache.size_bytes % cache.line_size != 0)
  {
    throw std::invalid_argument("cache size must be divisible by line size: " +
                                cache.name);
  }
  const CacheGeometry geometry{
    cache.line_size,
    cache.size_bytes / cache.line_size,
    cache.associativity,
  };
  cache_set_count(geometry);
  return geometry;
}

void validate_cache_config(const HierarchyConfig & config)
{
  if (config.schema_version != 1)
  {
    throw std::invalid_argument("unsupported cache schema version: " +
                                std::to_string(config.schema_version));
  }
  if (config.num_cores == 0)
  {
    throw std::invalid_argument(
      "cache hierarchy must define at least one core");
  }
  if (config.memory.name.empty())
  {
    throw std::invalid_argument("memory name must not be empty");
  }
  if (config.caches.empty())
  {
    throw std::invalid_argument(
      "cache hierarchy must define at least one cache");
  }

  std::unordered_map<std::string, const CacheConfig *> caches;
  for (const auto & cache : config.caches)
  {
    if (cache.name.empty() || cache.role.empty())
    {
      throw std::invalid_argument("cache name and role must not be empty");
    }
    if (cache.name == config.memory.name ||
        !caches.emplace(cache.name, &cache).second)
    {
      throw std::invalid_argument("duplicate cache or memory name: " +
                                  cache.name);
    }
    if (cache.private_to && *cache.private_to >= config.num_cores)
    {
      throw std::invalid_argument("cache private_to is outside core range: " +
                                  cache.name);
    }
    if (cache.next.empty())
    {
      throw std::invalid_argument("cache next target must not be empty: " +
                                  cache.name);
    }
    make_cache_geometry(cache);
  }

  for (const auto & cache : config.caches)
  {
    if (cache.next != config.memory.name && caches.count(cache.next) == 0)
    {
      throw std::invalid_argument("unknown next cache for " + cache.name +
                                  ": " + cache.next);
    }
  }

  if (config.core_mappings.size() != config.num_cores)
  {
    throw std::invalid_argument("every configured core needs one L1 mapping");
  }
  std::vector<bool> mapped(config.num_cores, false);
  for (const auto & mapping : config.core_mappings)
  {
    if (mapping.id >= config.num_cores || mapped[mapping.id])
    {
      throw std::invalid_argument("invalid or duplicate core mapping: " +
                                  std::to_string(mapping.id));
    }
    mapped[mapping.id] = true;
    const auto & entry = find_cache_config(config, mapping.l1);
    if (entry.role != "L1" || !entry.private_to ||
        *entry.private_to != mapping.id)
    {
      throw std::invalid_argument(
        "core mapping must reference its private L1: " + mapping.l1);
    }
  }

  for (const auto & cache : config.caches)
  {
    std::unordered_set<std::string> visited;
    const CacheConfig * current = &cache;
    while (current->next != config.memory.name)
    {
      if (!visited.emplace(current->name).second)
      {
        throw std::invalid_argument("cache hierarchy contains a cycle at: " +
                                    current->name);
      }
      current = caches.at(current->next);
    }
  }
}

}  // namespace yarda
