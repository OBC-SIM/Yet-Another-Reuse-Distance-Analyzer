#include "unroller.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
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

AccessOperation access_operation(const Json & node)
{
  const auto operation = node.value("op", "");
  if (operation == "load")
  {
    return AccessOperation::Load;
  }
  if (operation == "store")
  {
    return AccessOperation::Store;
  }
  return AccessOperation::Unknown;
}

std::optional<ResolvedAccess> resolve_access(
  const Json & node, const std::vector<std::string> & indices,
  std::uint64_t source_access_ordinal, const ObjectAddressModel & objects,
  const AccessLayoutResolver & layouts)
{
  const auto object_id = node.value("object", "");
  if (object_id.rfind("global::", 0) != 0)
  {
    return std::nullopt;
  }
  const auto access = layouts.resolve(node, indices);
  if (!access)
  {
    throw std::invalid_argument("global access layout is unresolved: " +
                                object_id);
  }
  if (access->offset < 0)
  {
    throw std::invalid_argument("global access offset is negative: " +
                                object_id);
  }
  const auto offset = static_cast<std::uint64_t>(access->offset);
  const auto size = static_cast<std::uint64_t>(access->size);
  const auto object = objects.objects.find(object_id);
  if (object == objects.objects.end())
  {
    throw std::invalid_argument("ELF object is unresolved: " + object_id);
  }
  if (offset >= object->second.size || size > object->second.size - offset)
  {
    throw std::invalid_argument("access exceeds ELF object extent: " +
                                object_id);
  }
  if (object->second.base > std::numeric_limits<std::uint64_t>::max() - offset)
  {
    throw std::overflow_error("ELF object address overflow: " + object_id);
  }
  const auto address = object->second.base + offset;
  if (size - 1 > std::numeric_limits<std::uint64_t>::max() - address)
  {
    throw std::overflow_error("ELF object address overflow: " + object_id);
  }
  return ResolvedAccess{object_id,
                        offset,
                        size,
                        address,
                        objects.basis,
                        access_operation(node),
                        source_access_ordinal};
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
                             std::size_t cache_line_size,
                             const AccessLayoutResolver & layouts)
  : granularity_(granularity)
  , cache_line_size_(cache_line_size)
  , layouts_(layouts)
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
        trace_cache_line_keys(access, indices, cache_line_size_, layouts_);
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

ResolvedTraceUnroller::ResolvedTraceUnroller(
  const ObjectAddressModel & objects, const AccessLayoutResolver & layouts,
  ScalarAccessPolicy scalar_policy)
  : objects_(objects), layouts_(layouts), scalar_policy_(scalar_policy)
{
}

std::vector<ResolvedAccess>
ResolvedTraceUnroller::unroll(const nlohmann::json & node)
{
  std::vector<ResolvedAccess> accesses;
  const auto emit = [&](const Json & access,
                        const std::vector<std::string> & indices) {
    const auto ordinal = next_source_access_ordinal_++;
    if (scalar_policy_ == ScalarAccessPolicy::Omit &&
        access.value("type", "") == "Scalar")
    {
      return;
    }
    auto resolved =
      resolve_access(access, indices, ordinal, objects_, layouts_);
    if (resolved)
    {
      accesses.push_back(std::move(*resolved));
    }
  };
  visit_node(node, {}, emit);
  return accesses;
}

}  // namespace yarda::detail
