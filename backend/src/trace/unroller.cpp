#include "unroller.hpp"

#include <cstdint>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace yarda::detail
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
    const auto count = iteration_count(start, bound, step);
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

}  // namespace

TraceUnroller::TraceUnroller(Granularity granularity,
                             std::size_t cache_line_size)
  : granularity_(granularity), cache_line_size_(cache_line_size)
{
}

std::vector<std::string>
TraceUnroller::unroll(const nlohmann::json & node) const
{
  std::vector<std::string> trace;
  const auto emit = [&](const Json & access,
                        const std::vector<std::string> & indices) {
    if (access.value("type", "") == "Scalar")
    {
      trace.push_back(access.value("name", ""));
      return;
    }
    if (granularity_ == Granularity::CacheLine)
    {
      const auto cache_lines =
        trace_cache_line_keys(access, indices, cache_line_size_);
      if (!cache_lines.empty())
      {
        trace.insert(trace.end(), cache_lines.begin(), cache_lines.end());
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

MappedTraceUnroller::MappedTraceUnroller(const CacheGeometry & geometry,
                                         const ObjectAddressModel & objects)
  : mapper_(geometry, objects)
{
}

std::vector<CacheLineMapping>
MappedTraceUnroller::unroll(const nlohmann::json & node) const
{
  std::vector<CacheLineMapping> accesses;
  const auto emit = [&](const Json & access,
                        const std::vector<std::string> & indices) {
    if (access.value("type", "") == "Scalar")
    {
      return;
    }
    auto mappings = mapper_.map(access, indices);
    accesses.insert(accesses.end(),
                    std::make_move_iterator(mappings.begin()),
                    std::make_move_iterator(mappings.end()));
  };
  visit_node(node, {}, emit);
  return accesses;
}

}  // namespace yarda::detail
