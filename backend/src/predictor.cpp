#include "yarda/predictor.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "predictor_internal.hpp"
#include "yarda/trace/calls.hpp"
#include "yarda/dilation.hpp"
#include "yarda/merger.hpp"
#include "yarda/trace/trace.hpp"

namespace yarda
{
namespace
{

using Json = nlohmann::json;
using Distances = std::set<std::size_t>;
using detail::LoopPrediction;

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

std::size_t loop_depth(const Json & node)
{
  if (node.value("type", "") != "Loop")
  {
    return 0;
  }
  std::size_t child_depth = 0;
  for (const auto & child : node.value("body", Json::array()))
  {
    child_depth = std::max(child_depth, loop_depth(child));
  }
  return child_depth + 1;
}

std::vector<std::uint64_t> target_bounds(const Json & root)
{
  std::vector<std::uint64_t> bounds;
  const Json * current = &root;
  while (current != nullptr && current->value("type", "") == "Loop")
  {
    bounds.push_back(iteration_count(current->value("start", 0LL),
                                     current->at("bound").get<std::int64_t>(),
                                     current->value("step", 1LL)));
    const Json * next = nullptr;
    for (const auto & child : current->at("body"))
    {
      if (child.value("type", "") == "Loop")
      {
        next = &child;
        break;
      }
    }
    current = next;
  }
  return bounds;
}

LoopPrediction sample(const Json & node,
                      const std::vector<std::size_t> & bounds)
{
  auto trace = unroll_node_sample(node, bounds);
  return {calculate_reuse_profile(trace), std::move(trace)};
}

std::uint64_t frequency(const ReuseProfile & profile, std::size_t distance)
{
  const auto found = profile.histogram.find(distance);
  return found == profile.histogram.end() ? 0 : found->second;
}

Distances keys(const ReuseProfile & profile)
{
  Distances result;
  for (const auto & [distance, unused] : profile.histogram)
  {
    static_cast<void>(unused);
    result.insert(distance);
  }
  return result;
}

Distances union_keys(const std::vector<const ReuseProfile *> & profiles)
{
  Distances result;
  for (const auto * profile : profiles)
  {
    const auto profile_keys = keys(*profile);
    result.insert(profile_keys.begin(), profile_keys.end());
  }
  return result;
}

SignedHistogram difference(const ReuseProfile & left,
                           const ReuseProfile & right,
                           const Distances & distances)
{
  SignedHistogram result;
  for (const auto distance : distances)
  {
    result[distance] = static_cast<std::int64_t>(frequency(left, distance)) -
                       static_cast<std::int64_t>(frequency(right, distance));
  }
  return result;
}

LoopPrediction predict_1d(const Json & node,
                          const std::vector<std::uint64_t> & target)
{
  const auto exact_trace = unroll_node_actual(node);
  const auto exact = calculate_reuse_profile(exact_trace);
  if (target[0] <= 1)
  {
    return {exact, exact_trace};
  }
  const auto base = sample(node, {1});
  const auto b2 = sample(node, {2});
  const auto distances = union_keys({&base.profile, &b2.profile});
  ReuseProfile predicted;
  for (const auto distance : distances)
  {
    const auto initial =
      static_cast<std::int64_t>(frequency(base.profile, distance));
    const auto increment =
      static_cast<std::int64_t>(frequency(b2.profile, distance)) - initial;
    const auto value =
      initial + static_cast<std::int64_t>(target[0] - 1) * increment;
    if (value > 0)
    {
      predicted.histogram[distance] = static_cast<std::uint64_t>(value);
    }
  }
  predicted.cold_misses = exact.cold_misses;
  return {predicted, exact_trace};
}

LoopPrediction predict_2d(const Json & node,
                          const std::vector<std::uint64_t> & target)
{
  const auto b22 = sample(node, {2, 2});
  const auto b32 = sample(node, {3, 2});
  const auto b23 = sample(node, {2, 3});
  const auto b33 = sample(node, {3, 3});
  const auto b44 = sample(node, {4, 4});
  const auto distances =
    union_keys({&b22.profile, &b32.profile, &b23.profile, &b33.profile});
  const auto incr_j = difference(b32.profile, b22.profile, distances);
  const auto incr_k = difference(b23.profile, b22.profile, distances);
  SignedHistogram coefficient;
  Distances stable;
  for (const auto distance : distances)
  {
    coefficient[distance] =
      static_cast<std::int64_t>(frequency(b33.profile, distance)) -
      static_cast<std::int64_t>(frequency(b22.profile, distance)) -
      incr_j.at(distance) - incr_k.at(distance);
    const auto expected =
      static_cast<std::int64_t>(frequency(b22.profile, distance)) +
      2 * incr_j.at(distance) + 2 * incr_k.at(distance) +
      4 * coefficient.at(distance);
    if (frequency(b22.profile, distance) > 0 &&
        frequency(b32.profile, distance) > 0 &&
        frequency(b23.profile, distance) > 0 &&
        frequency(b33.profile, distance) > 0 &&
        static_cast<std::int64_t>(frequency(b44.profile, distance)) == expected)
    {
      stable.insert(distance);
    }
  }
  DilationContext context{{target[0], target[1]}, {}, {}};
  for (const auto distance : stable)
  {
    context.base.histogram[distance] = frequency(b22.profile, distance);
  }
  context.coefficients = {
    {"Incr_J", incr_j}, {"Incr_K", incr_k}, {"Coff_JK", coefficient}};
  auto predicted = dilate(2, context);
  const auto exact_trace = unroll_node_actual(node);
  const auto exact = calculate_reuse_profile(exact_trace);
  for (const auto & [distance, count] : exact.histogram)
  {
    if (!stable.count(distance))
    {
      predicted.histogram[distance] = count;
    }
  }
  predicted.cold_misses = exact.cold_misses;
  return {predicted, b22.trace};
}

LoopPrediction predict_loop(const Json & node)
{
  const auto depth = loop_depth(node);
  const auto target = target_bounds(node);
  if (depth == 1) return predict_1d(node, target);
  if (depth == 2) return predict_2d(node, target);
  if (depth == 3) return detail::predict_3d_loop(node, target);
  throw std::invalid_argument(std::to_string(depth) + "D loop is not "
                                                      "supported");
}

}  // namespace

PredictionResult predict_module(const nlohmann::json & raw)
{
  const auto module = expand_calls(raw);
  PredictionResult result;
  BlockMerger merger;
  for (const auto & function : module)
  {
    const auto function_name = function.at("function").get<std::string>();
    std::vector<std::string> flat;
    const auto flush = [&]() {
      if (flat.empty()) return;
      const auto profile = calculate_reuse_profile(flat);
      result.blocks.push_back({function_name + "  (flat, " +
                                 std::to_string(flat.size()) + " accesses)",
                               profile});
      merger.merge(profile, flat);
      flat.clear();
    };
    for (const auto & node : function.value("body", Json::array()))
    {
      if (node.value("type", "") == "Loop")
      {
        flush();
        auto prediction = predict_loop(node);
        result.blocks.push_back(
          {function_name + "  " + node.value("var", "") +
             "-loop (bound=" + std::to_string(node.value("bound", 0)) + ")",
           prediction.profile});
        merger.merge(prediction.profile, prediction.trace);
      }
      else
      {
        auto trace = unroll_node_actual(node);
        flat.insert(flat.end(), trace.begin(), trace.end());
      }
    }
    flush();
  }
  result.program = merger.profile();
  return result;
}

}  // namespace yarda
