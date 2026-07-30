#include "yarda/trace.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

std::optional<std::int64_t> parse_integer(const std::string & value)
{
  try
  {
    std::size_t consumed = 0;
    const auto parsed = std::stoll(value, &consumed);
    return consumed == value.size() ? std::optional<std::int64_t>(parsed)
                                    : std::nullopt;
  }
  catch (const std::exception &)
  {
    return std::nullopt;
  }
}

std::int64_t floor_divide(std::int64_t dividend, std::int64_t divisor)
{
  auto quotient = dividend / divisor;
  const auto remainder = dividend % divisor;
  if (remainder != 0 && dividend < 0)
  {
    --quotient;
  }
  return quotient;
}

std::optional<std::string>
cache_line_key(const Json & node, const std::vector<std::string> & indices,
               std::size_t line_size)
{
  if (!node.contains("elem_size") || line_size == 0)
  {
    return std::nullopt;
  }
  std::vector<std::int64_t> numeric;
  for (const auto & index : indices)
  {
    const auto parsed = parse_integer(index);
    if (!parsed)
    {
      return std::nullopt;
    }
    numeric.push_back(*parsed);
  }

  std::int64_t linear = 0;
  if (numeric.size() == 1)
  {
    linear = numeric.front();
  }
  else if (node.contains("shape") && node["shape"].is_array() &&
           (node["shape"].size() == numeric.size() ||
            node["shape"].size() + 1 == numeric.size()))
  {
    std::vector<std::int64_t> shape;
    const auto required = numeric.size() - 1;
    const auto start = node["shape"].size() - required;
    for (std::size_t index = start; index < node["shape"].size(); ++index)
    {
      shape.push_back(node["shape"][index].get<std::int64_t>());
    }
    for (std::size_t position = 0; position < numeric.size(); ++position)
    {
      std::int64_t stride = 1;
      for (std::size_t dimension = position; dimension < shape.size();
           ++dimension)
      {
        stride *= shape[dimension];
      }
      linear += numeric[position] * stride;
    }
  }
  else
  {
    return std::nullopt;
  }
  const auto byte_offset = linear * node["elem_size"].get<std::int64_t>();
  return node.value("name", "") + "-line-" +
         std::to_string(
           floor_divide(byte_offset, static_cast<std::int64_t>(line_size)));
}

void append_node(const Json & node, const Environment & environment,
                 Granularity granularity, std::size_t line_size,
                 std::vector<std::string> & trace,
                 const std::vector<std::size_t> * simulation_bounds = nullptr,
                 std::size_t loop_level = 0)
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
      if (const auto key = cache_line_key(node, indices, line_size))
      {
        trace.push_back(*key);
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
                    simulation_bounds, loop_level + 1);
      }
    }
    return;
  }
  throw std::invalid_argument("Unknown LAT node type: " + type);
}

}  // namespace

std::vector<std::string> unroll_node_actual(const nlohmann::json & node,
                                            Granularity granularity,
                                            std::size_t cache_line_size)
{
  std::vector<std::string> trace;
  append_node(node, {}, granularity, cache_line_size, trace);
  return trace;
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
  const auto module = expand_calls(raw);
  std::vector<NamedTrace> result;
  for (const auto & function : module)
  {
    const auto function_name = function.at("function").get<std::string>();
    std::vector<std::string> flat;
    const auto flush_flat = [&]() {
      if (!flat.empty())
      {
        result.push_back({function_name + "  (flat, " +
                            std::to_string(flat.size()) + " accesses)",
                          std::move(flat)});
        flat.clear();
      }
    };
    for (const auto & node : function.value("body", Json::array()))
    {
      auto trace = unroll_node_actual(node, granularity, cache_line_size);
      if (node.value("type", "") == "Loop")
      {
        flush_flat();
        result.push_back(
          {function_name + "  " + node.value("var", "") +
             "-loop (bound=" + std::to_string(node.value("bound", 0)) + ")",
           std::move(trace)});
      }
      else
      {
        flat.insert(flat.end(), trace.begin(), trace.end());
      }
    }
    flush_flat();
  }
  return result;
}

}  // namespace yarda
