#include "yarda/trace/mapped_trace.hpp"

#include <iterator>
#include <utility>
#include <vector>

#include "block_trace.hpp"
#include "unroller.hpp"
#include "yarda/trace/resolved_mapping.hpp"

namespace yarda
{
namespace
{

void validate_mapping_geometry(const CacheGeometry & geometry)
{
  static_cast<void>(cache_set_count(geometry));
}

std::vector<CacheLineMapping> map_resolved_accesses(
  const std::vector<ResolvedAccess> & accesses, const CacheGeometry & geometry)
{
  std::vector<CacheLineMapping> result;
  for (const auto & access : accesses)
  {
    auto mappings = map_cache_lines(access, geometry);
    result.insert(result.end(), std::make_move_iterator(mappings.begin()),
                  std::make_move_iterator(mappings.end()));
  }
  return result;
}

void collect_mappings(const std::vector<NamedMappedTrace> & traces,
                      CacheLineMappingTable & mappings)
{
  for (const auto & trace : traces)
  {
    for (const auto & mapping : trace.accesses)
    {
      mappings.emplace(
        std::make_pair(mapping.object_id, mapping.object_byte_offset),
        CacheLineAddressMapping{mapping.object_id, mapping.object_byte_offset,
                                mapping.address_basis, mapping.decoded});
    }
  }
}

}  // namespace

std::vector<CacheLineMapping>
flatten_mapped_traces(const std::vector<NamedMappedTrace> & traces)
{
  std::vector<CacheLineMapping> result;
  for (const auto & trace : traces)
  {
    result.insert(result.end(), trace.accesses.begin(), trace.accesses.end());
  }
  return result;
}

std::vector<CacheLineMapping>
unroll_node_actual(const nlohmann::json & node, const CacheGeometry & geometry,
                   const ObjectAddressModel & objects)
{
  validate_mapping_geometry(geometry);
  const detail::AccessLayoutResolver layouts;
  detail::ResolvedTraceUnroller unroller(objects, layouts,
                                         detail::ScalarAccessPolicy::Omit);
  return map_resolved_accesses(unroller.unroll(node), geometry);
}

MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects)
{
  validate_mapping_geometry(geometry);
  const detail::AccessLayoutResolver layouts(raw);
  detail::ResolvedTraceUnroller unroller(objects, layouts,
                                         detail::ScalarAccessPolicy::Omit);
  MappedTraceResult result;
  result.traces =
    detail::build_block_traces<NamedMappedTrace, CacheLineMapping>(
      raw,
      [&unroller, &geometry](const nlohmann::json & node) {
        return map_resolved_accesses(unroller.unroll(node), geometry);
      },
      detail::EmptyLoopPolicy::Omit);
  collect_mappings(result.traces, result.mappings);
  return result;
}

}  // namespace yarda
