#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "predictor_internal.hpp"
#include "yarda/dilation.hpp"
#include "yarda/trace.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;
using Distances = std::set<std::size_t>;
using Key = std::array<int, 3>;

LoopPrediction sample(const Json & node, const Key & bounds)
{
  auto trace = unroll_node_sample(node, {static_cast<std::size_t>(bounds[0]),
                                         static_cast<std::size_t>(bounds[1]),
                                         static_cast<std::size_t>(bounds[2])});
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

Distances union_keys(const std::map<Key, LoopPrediction> & samples)
{
  Distances result;
  for (const auto & [unused, prediction] : samples)
  {
    static_cast<void>(unused);
    const auto current = keys(prediction.profile);
    result.insert(current.begin(), current.end());
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

}  // namespace

LoopPrediction predict_3d_loop(const nlohmann::json & node,
                               const std::vector<std::uint64_t> & target)
{
  std::map<Key, LoopPrediction> samples;
  for (int i : {2, 3})
    for (int j : {2, 3})
      for (int k : {2, 3}) samples[{i, j, k}] = sample(node, {i, j, k});

  const auto & base = samples.at({2, 2, 2}).profile;
  const auto distances = union_keys(samples);
  Distances stable = keys(base);
  for (const auto & [unused, prediction] : samples)
  {
    static_cast<void>(unused);
    Distances intersection;
    const auto current = keys(prediction.profile);
    std::set_intersection(stable.begin(), stable.end(), current.begin(),
                          current.end(),
                          std::inserter(intersection, intersection.begin()));
    stable = std::move(intersection);
  }

  const auto diff = [&](const Key & key) {
    return difference(samples.at(key).profile, base, distances);
  };
  const auto incr_i = diff({3, 2, 2});
  const auto incr_j = diff({2, 3, 2});
  const auto incr_k = diff({2, 2, 3});
  SignedHistogram cij, cik, cjk, cijk;
  for (const auto distance : distances)
  {
    cij[distance] = static_cast<std::int64_t>(
                      frequency(samples.at({3, 3, 2}).profile, distance)) -
                    static_cast<std::int64_t>(frequency(base, distance)) -
                    incr_i.at(distance) - incr_j.at(distance);
    cik[distance] = static_cast<std::int64_t>(
                      frequency(samples.at({3, 2, 3}).profile, distance)) -
                    static_cast<std::int64_t>(frequency(base, distance)) -
                    incr_i.at(distance) - incr_k.at(distance);
    cjk[distance] = static_cast<std::int64_t>(
                      frequency(samples.at({2, 3, 3}).profile, distance)) -
                    static_cast<std::int64_t>(frequency(base, distance)) -
                    incr_j.at(distance) - incr_k.at(distance);
    cijk[distance] = static_cast<std::int64_t>(
                       frequency(samples.at({3, 3, 3}).profile, distance)) -
                     static_cast<std::int64_t>(frequency(base, distance)) -
                     incr_i.at(distance) - incr_j.at(distance) -
                     incr_k.at(distance) - cij.at(distance) - cik.at(distance) -
                     cjk.at(distance);
  }

  DilationContext context{{target[0], target[1], target[2]}, {}, {}};
  for (const auto distance : stable)
    context.base.histogram[distance] = frequency(base, distance);
  context.coefficients = {{"Incr_I", incr_i}, {"Incr_J", incr_j},
                          {"Incr_K", incr_k}, {"Coff_IJ", cij},
                          {"Coff_IK", cik},   {"Coff_JK", cjk},
                          {"Coff_IJK", cijk}};
  auto predicted = dilate(3, context);

  const auto exact_trace = unroll_node_actual(node);
  const auto exact = calculate_reuse_profile(exact_trace);
  for (const auto & [distance, count] : exact.histogram)
    if (!stable.count(distance)) predicted.histogram[distance] = count;
  predicted.cold_misses = exact.cold_misses;
  return {predicted, samples.at({2, 2, 2}).trace};
}

}  // namespace yarda::detail
