#include "yarda/trace.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "trace/cache_line.hpp"
#include "yarda/calls.hpp"

namespace yarda
{
namespace
{

using Json = nlohmann::json;
using Environment = std::unordered_map<std::string, std::int64_t>;

std::string resolve_index(const std::string & index,
                          const Environment & environment)
{
  if (const auto exact = environment.find(index); exact != environment.end())
  {
    return std::to_string(exact->second);
  }
  const auto operator_position = index.find_first_of("+-", 1);
  if (operator_position != std::string::npos)
  {
    const auto base_name = index.substr(0, operator_position);
    if (const auto base = environment.find(base_name);
        base != environment.end())
    {
      try
      {
        std::size_t consumed = 0;
        const auto suffix = index.substr(operator_position);
        const auto offset = std::stoll(suffix, &consumed);
        if (consumed == suffix.size())
        {
          return std::to_string(base->second + offset);
        }
      }
      catch (const std::exception &)
      {
        return index;
      }
    }
  }
  return index;
}

std::uint64_t iteration_count(std::int64_t start, std::int64_t bound,
                              std::int64_t step)
{
  step = step == 0 ? 1 : step;
  if (step > 0)
  {
    return start >= bound
             ? 0
             : static_cast<std::uint64_t>((bound - start + step - 1) / step);
  }
  const auto magnitude = -step;
  return start <= bound ? 0
                        : static_cast<std::uint64_t>(
                            (start - bound + magnitude - 1) / magnitude);
}

void append_node(const Json & node, const Environment & environment,
                 Granularity granularity, std::size_t line_size,
                 std::vector<std::string> & trace,
                 const std::vector<std::size_t> * simulation_bounds = nullptr,
                 std::size_t loop_level = 0,
                 const CacheGeometry * geometry = nullptr,
                 const ObjectAddressModel * objects = nullptr,
                 CacheLineMappingTable * mappings = nullptr,
                 std::vector<CacheLineMapping> * mapped_accesses = nullptr)
{
  const auto type = node.value("type", "");
  if (type == "Scalar")
  {
    trace.push_back(node.value("name", ""));
    return;
  }
  if (type == "Array")
  {
    std::vector<std::string> indices;
    for (const auto & index : node.value("indices", Json::array()))
    {
      indices.push_back(resolve_index(index.get<std::string>(), environment));
    }
    if (granularity == Granularity::CacheLine)
    {
      if (const auto cache_line = detail::trace_cache_line(
            node, indices, line_size, geometry, objects, mappings))
      {
        trace.push_back(cache_line->key);
        if (mapped_accesses != nullptr && cache_line->mapping)
        {
          mapped_accesses->push_back(*cache_line->mapping);
        }
        return;
      }
    }
    std::ostringstream key;
    key << node.value("name", "");
    for (const auto & index : indices)
    {
      key << '-' << index;
    }
    trace.push_back(key.str());
    return;
  }
  if (type == "Loop")
  {
    const auto variable = node.at("var").get<std::string>();
    const auto start = node.value("start", 0LL);
    const auto bound = node.at("bound").get<std::int64_t>();
    const auto step =
      node.value("step", 1LL) == 0 ? 1LL : node.value("step", 1LL);
    auto count = iteration_count(start, bound, step);
    if (simulation_bounds != nullptr && loop_level < simulation_bounds->size())
    {
      count = std::min<std::uint64_t>(count, (*simulation_bounds)[loop_level]);
    }
    for (std::uint64_t iteration = 0; iteration < count; ++iteration)
    {
      auto child_environment = environment;
      child_environment[variable] =
        start + static_cast<std::int64_t>(iteration) * step;
      for (const auto & child : node.value("body", Json::array()))
      {
        append_node(child, child_environment, granularity, line_size, trace,
                    simulation_bounds, loop_level + 1, geometry, objects,
                    mappings, mapped_accesses);
      }
    }
    return;
  }
  throw std::invalid_argument("Unknown LAT node type: " + type);
}

std::vector<std::string> unroll_node(
  const Json & node, Granularity granularity, std::size_t cache_line_size,
  const CacheGeometry * geometry = nullptr,
  const ObjectAddressModel * objects = nullptr,
  CacheLineMappingTable * mappings = nullptr,
  std::vector<CacheLineMapping> * mapped_accesses = nullptr)
{
  std::vector<std::string> trace;
  append_node(node, {}, granularity, cache_line_size, trace, nullptr, 0,
              geometry, objects, mappings, mapped_accesses);
  return trace;
}

std::vector<NamedTrace> block_traces_impl(
  const nlohmann::json & raw, Granularity granularity,
  std::size_t cache_line_size, const CacheGeometry * geometry,
  const ObjectAddressModel * objects, CacheLineMappingTable * mappings,
  std::vector<NamedMappedTrace> * mapped_traces = nullptr)
{
  const auto module = expand_calls(raw);
  std::vector<NamedTrace> result;
  for (const auto & function : module)
  {
    const auto function_name = function.at("function").get<std::string>();
    std::vector<std::string> flat;
    std::vector<CacheLineMapping> flat_mapped;
    const auto flush_flat = [&]() {
      if (!flat.empty())
      {
        const auto name = function_name + "  (flat, " +
                          std::to_string(flat.size()) + " accesses)";
        result.push_back({name, std::move(flat)});
        if (mapped_traces != nullptr && !flat_mapped.empty())
        {
          mapped_traces->push_back({name, std::move(flat_mapped)});
        }
        flat.clear();
        flat_mapped.clear();
      }
    };
    for (const auto & node : function.value("body", Json::array()))
    {
      std::vector<CacheLineMapping> node_mapped;
      auto trace = unroll_node(node, granularity, cache_line_size, geometry,
                               objects, mappings, &node_mapped);
      if (node.value("type", "") == "Loop")
      {
        flush_flat();
        const auto name =
          function_name + "  " + node.value("var", "") +
          "-loop (bound=" + std::to_string(node.value("bound", 0)) + ")";
        result.push_back({name, std::move(trace)});
        if (mapped_traces != nullptr && !node_mapped.empty())
        {
          mapped_traces->push_back({name, std::move(node_mapped)});
        }
      }
      else
      {
        flat.insert(flat.end(), trace.begin(), trace.end());
        flat_mapped.insert(flat_mapped.end(), node_mapped.begin(),
                           node_mapped.end());
      }
    }
    flush_flat();
  }
  return result;
}

}  // namespace

std::vector<std::string> unroll_node_actual(const nlohmann::json & node,
                                            Granularity granularity,
                                            std::size_t cache_line_size)
{
  return unroll_node(node, granularity, cache_line_size);
}

std::vector<std::string> unroll_node_actual(const nlohmann::json & node,
                                            const CacheGeometry & geometry,
                                            const ObjectAddressModel & objects)
{
  cache_set_count(geometry);
  return unroll_node(node, Granularity::CacheLine, geometry.line_size,
                     &geometry, &objects);
}

std::vector<std::string>
unroll_node_sample(const nlohmann::json & node,
                   const std::vector<std::size_t> & simulation_bounds)
{
  std::vector<std::string> trace;
  append_node(node, {}, Granularity::Element, 32, trace, &simulation_bounds, 0);
  return trace;
}

std::vector<NamedTrace> block_traces(const nlohmann::json & raw,
                                     Granularity granularity,
                                     std::size_t cache_line_size)
{
  return block_traces_impl(raw, granularity, cache_line_size, nullptr, nullptr,
                           nullptr, nullptr);
}

MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects)
{
  cache_set_count(geometry);
  MappedTraceResult result;
  result.traces = block_traces_impl(raw, Granularity::CacheLine,
                                    geometry.line_size, &geometry, &objects,
                                    &result.mappings, &result.mapped_traces);
  return result;
}

}  // namespace yarda
