#include "yarda/trace/trace.hpp"

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "cache_line.hpp"
#include "yarda/trace/calls.hpp"

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

template<typename Emit>
void visit_node(const Json & node, const Environment & environment,
                const Emit & emit)
{
  const auto type = node.value("type", "");
  if (type == "Scalar")
  {
    emit(node, std::vector<std::string>{});
    return;
  }
  if (type == "Array")
  {
    std::vector<std::string> indices;
    for (const auto & index : node.value("indices", Json::array()))
    {
      indices.push_back(resolve_index(index.get<std::string>(), environment));
    }
    emit(node, indices);
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
    for (std::uint64_t iteration = 0; iteration < count; ++iteration)
    {
      auto child_environment = environment;
      child_environment[variable] =
        start + static_cast<std::int64_t>(iteration) * step;
      for (const auto & child : node.value("body", Json::array()))
      {
        visit_node(child, child_environment, emit);
      }
    }
    return;
  }
  throw std::invalid_argument("Unknown LAT node type: " + type);
}

std::vector<std::string> unroll_node(
  const Json & node, Granularity granularity, std::size_t cache_line_size)
{
  std::vector<std::string> trace;
  const auto emit = [&](const Json & access,
                        const std::vector<std::string> & indices) {
    if (access.value("type", "") == "Scalar")
    {
      trace.push_back(access.value("name", ""));
      return;
    }
    if (granularity == Granularity::CacheLine)
    {
      const auto cache_line = detail::trace_cache_line(
        access, indices, cache_line_size, nullptr, nullptr, nullptr);
      if (cache_line && cache_line->key)
      {
        trace.push_back(*cache_line->key);
        return;
      }
    }
    std::ostringstream key;
    key << access.value("name", "");
    for (const auto & index : indices)
    {
      key << '-' << index;
    }
    trace.push_back(key.str());
  };
  visit_node(node, {}, emit);
  return trace;
}

std::vector<CacheLineMapping>
unroll_mapped_node(const Json & node, const CacheGeometry & geometry,
                   const ObjectAddressModel & objects,
                   CacheLineMappingTable * mappings)
{
  std::vector<CacheLineMapping> accesses;
  const auto emit = [&](const Json & access,
                        const std::vector<std::string> & indices) {
    if (access.value("type", "") == "Scalar")
    {
      return;
    }
    const auto cache_line = detail::trace_cache_line(
      access, indices, geometry.line_size, &geometry, &objects, mappings);
    if (cache_line && cache_line->mapping)
    {
      accesses.push_back(*cache_line->mapping);
    }
  };
  visit_node(node, {}, emit);
  return accesses;
}

template<typename Named, typename Access, typename Unroll>
std::vector<Named> block_traces_impl(const nlohmann::json & raw,
                                     Unroll unroll, bool include_empty_loops)
{
  const auto module = expand_calls(raw);
  std::vector<Named> result;
  for (const auto & function : module)
  {
    const auto function_name = function.at("function").get<std::string>();
    std::vector<Access> flat;
    const auto flush_flat = [&]() {
      if (!flat.empty())
      {
        const auto name = function_name + "  (flat, " +
                          std::to_string(flat.size()) + " accesses)";
        result.push_back({name, std::move(flat)});
        flat.clear();
      }
    };
    for (const auto & node : function.value("body", Json::array()))
    {
      auto accesses = unroll(node);
      if (node.value("type", "") == "Loop")
      {
        flush_flat();
        const auto name =
          function_name + "  " + node.value("var", "") +
          "-loop (bound=" + std::to_string(node.value("bound", 0)) + ")";
        if (include_empty_loops || !accesses.empty())
        {
          result.push_back({name, std::move(accesses)});
        }
      }
      else
      {
        flat.insert(flat.end(), accesses.begin(), accesses.end());
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

std::vector<CacheLineMapping>
unroll_node_actual(const nlohmann::json & node, const CacheGeometry & geometry,
                   const ObjectAddressModel & objects)
{
  cache_set_count(geometry);
  return unroll_mapped_node(node, geometry, objects, nullptr);
}

std::vector<NamedTrace> block_traces(const nlohmann::json & raw,
                                     Granularity granularity,
                                     std::size_t cache_line_size)
{
  return block_traces_impl<NamedTrace, std::string>(
    raw,
    [granularity, cache_line_size](const Json & node) {
      return unroll_node(node, granularity, cache_line_size);
    },
    true);
}

MappedTraceResult mapped_block_traces(const nlohmann::json & raw,
                                      const CacheGeometry & geometry,
                                      const ObjectAddressModel & objects)
{
  cache_set_count(geometry);
  MappedTraceResult result;
  result.traces = block_traces_impl<NamedMappedTrace, CacheLineMapping>(
    raw,
    [&geometry, &objects, &result](const Json & node) {
      return unroll_mapped_node(node, geometry, objects, &result.mappings);
    },
    false);
  return result;
}

}  // namespace yarda
