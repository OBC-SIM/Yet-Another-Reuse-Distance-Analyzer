#include "yarda/trace/mapped_trace.hpp"

#include <iterator>
#include <stdexcept>
#include <string>
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

MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects)
{
  validate_mapping_geometry(geometry);
  try
  {
    const detail::AccessLayoutResolver layouts(raw);
    detail::ResolvedTraceUnroller unroller(objects, layouts);
    MappedTraceResult result;
    result.traces =
      detail::build_block_traces<NamedMappedTrace, CacheLineMapping>(
        raw,
        [&unroller, &geometry](const std::string & task_id,
                               const nlohmann::json & node) {
          return map_resolved_accesses(unroller.unroll(node, task_id),
                                       geometry);
        },
        detail::EmptyLoopPolicy::Omit);
    result.coverage = unroller.coverage();
    for (const auto & trace : result.traces)
    {
      result.coverage.emitted_line_references += trace.accesses.size();
    }
    collect_mappings(result.traces, result.mappings);
    return result;
  }
  catch (const nlohmann::json::exception & error)
  {
    throw std::invalid_argument("malformed LAT input: " +
                                std::string(error.what()));
  }
}

}  // namespace yarda
