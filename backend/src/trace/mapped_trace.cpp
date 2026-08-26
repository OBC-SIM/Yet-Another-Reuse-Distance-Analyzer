#include "yarda/trace/mapped_trace.hpp"

#include <utility>
#include <vector>

#include "block_trace.hpp"
#include "unroller.hpp"

namespace yarda
{
namespace
{

void collect_mappings(const std::vector<NamedMappedTrace> & traces,
                      CacheLineMappingTable & mappings)
{
  for (const auto & trace : traces)
  {
    for (const auto & mapping : trace.accesses)
    {
      mappings.emplace(
        std::make_pair(mapping.object_id, mapping.object_byte_offset), mapping);
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
  const detail::AccessLayoutResolver layouts;
  return detail::MappedTraceUnroller(geometry, objects, layouts).unroll(node);
}

MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects)
{
  const detail::AccessLayoutResolver layouts(raw);
  const detail::MappedTraceUnroller unroller(geometry, objects, layouts);
  MappedTraceResult result;
  result.traces =
    detail::build_block_traces<NamedMappedTrace, CacheLineMapping>(
      raw,
      [&unroller](const nlohmann::json & node) {
        return unroller.unroll(node);
      },
      detail::EmptyLoopPolicy::Omit);
  collect_mappings(result.traces, result.mappings);
  return result;
}

}  // namespace yarda
