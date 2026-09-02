#include "unroller.hpp"

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "access_resolver.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;
using Environment = std::unordered_map<std::string, std::int64_t>;

constexpr std::uint64_t kMaxLoopIterations = 1'000'000;

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
          std::int64_t resolved = 0;
          if (!__builtin_add_overflow(base->second, offset, &resolved))
          {
            return std::to_string(resolved);
          }
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

std::uint64_t positive_distance(std::int64_t lower, std::int64_t upper)
{
  if (lower < 0 && upper >= 0)
  {
    const auto below_zero =
      static_cast<std::uint64_t>(-(lower + 1)) + std::uint64_t{1};
    return below_zero + static_cast<std::uint64_t>(upper);
  }
  return static_cast<std::uint64_t>(upper - lower);
}

std::uint64_t step_magnitude(std::int64_t step)
{
  return static_cast<std::uint64_t>(-(step + 1)) + std::uint64_t{1};
}

std::uint64_t ceil_divide(std::uint64_t dividend, std::uint64_t divisor)
{
  return dividend / divisor + (dividend % divisor != 0 ? 1 : 0);
}

std::uint64_t iteration_count(std::int64_t start, std::int64_t bound,
                              std::int64_t step)
{
  if (step == 0)
  {
    throw std::invalid_argument("loop step must be non-zero");
  }
  std::uint64_t count = 0;
  if (step > 0)
  {
    if (start < bound)
    {
      count = ceil_divide(positive_distance(start, bound),
                          static_cast<std::uint64_t>(step));
    }
  }
  else if (start > bound)
  {
    count = ceil_divide(positive_distance(bound, start), step_magnitude(step));
  }
  if (count > kMaxLoopIterations)
  {
    throw std::invalid_argument("loop iteration count exceeds 1000000");
  }
  return count;
}

template <typename Emit>
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
    const auto step = node.value("step", 1LL);
    const auto count = iteration_count(start, bound, step);
    auto value = start;
    for (std::uint64_t iteration = 0; iteration < count; ++iteration)
    {
      auto child_environment = environment;
      child_environment[variable] = value;
      for (const auto & child : node.value("body", Json::array()))
      {
        visit_node(child, child_environment, emit);
      }
      if (iteration + 1 < count && __builtin_add_overflow(value, step, &value))
      {
        throw std::invalid_argument("loop iteration value overflows");
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
  const ObjectAddressModel & objects, const AccessLayoutResolver & layouts)
  : objects_(objects), layouts_(layouts)
{
}

std::vector<ResolvedAccess> ResolvedTraceUnroller::unroll(
  const nlohmann::json & node, const std::string & task_id)
{
  std::vector<ResolvedAccess> accesses;
  const auto emit = [&](const Json & access,
                        const std::vector<std::string> & indices) {
    const auto ordinal = coverage_.source_accesses++;
    accesses.push_back(resolve_access(access, indices, task_id, ordinal,
                                      objects_, layouts_, coverage_));
  };
  visit_node(node, {}, emit);
  return accesses;
}

const TraceCoverage & ResolvedTraceUnroller::coverage() const noexcept
{
  return coverage_;
}

}  // namespace yarda::detail
