#include "yarda/cache/hierarchy_model.hpp"

#include <initializer_list>
#include <stdexcept>

namespace yarda
{

AnalysisHierarchy select_analysis_hierarchy(const HierarchyConfig & config)
{
  validate_cache_config(config);
  const auto & l1 = entry_cache_config(config, 0);
  if (l1.next == config.memory.name)
  {
    throw std::invalid_argument(
      "analysis L1 next must reference an LLC cache: " + l1.name);
  }
  const auto & llc = find_cache_config(config, l1.next);
  if (llc.role != "LLC")
  {
    throw std::invalid_argument("analysis requires role=LLC for cache: " +
                                llc.name);
  }
  if (llc.private_to)
  {
    throw std::invalid_argument(
      "analysis LLC must be shared with private_to omitted: " + llc.name);
  }
  if (llc.next != config.memory.name)
  {
    throw std::invalid_argument(
      "analysis LLC next must reference configured Memory '" +
      config.memory.name + "': " + llc.name);
  }
  for (const auto * cache : {&l1, &llc})
  {
    if (cache->replacement != Replacement::LRU)
    {
      throw std::invalid_argument(
        "analysis requires replacement=LRU for cache: " + cache->name);
    }
    if (!cache->write_allocate)
    {
      throw std::invalid_argument(
        "analysis requires write_allocate=true for cache: " + cache->name);
    }
  }
  if (l1.line_size != llc.line_size)
  {
    throw std::invalid_argument("analysis requires matching line_size for " +
                                l1.name + " and " + llc.name);
  }
  return {0,
          {l1.name, l1.role, make_cache_geometry(l1)},
          {llc.name, llc.role, make_cache_geometry(llc)},
          config.memory.name};
}

}  // namespace yarda
