#include "yarda/cache/lru_rd_analysis.hpp"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yarda
{
namespace
{

class FenwickTree
{
public:
  explicit FenwickTree(std::size_t size) : tree_(size + 1, 0) {}

  void add(std::size_t position, std::int64_t delta)
  {
    while (position < tree_.size())
    {
      tree_[position] += delta;
      position += position & (~position + 1);
    }
  }

  [[nodiscard]] std::int64_t prefix_sum(std::size_t position) const
  {
    std::int64_t total = 0;
    while (position > 0)
    {
      total += tree_[position];
      position -= position & (~position + 1);
    }
    return total;
  }

private:
  std::vector<std::int64_t> tree_;
};

struct IndexedLine
{
  std::size_t input_index = 0;
  std::uint64_t tag = 0;
};

void record_reuse(LruRdAnalysis & result, LruSetResult & set,
                  std::size_t input_index, std::size_t distance,
                  std::uint64_t associativity)
{
  ++result.histogram[distance];
  ++set.histogram[distance];
  auto & access = result.accesses[input_index];
  access.reuse_distance = distance;
  if (distance < associativity)
  {
    access.outcome = LruAccessOutcome::Hit;
    ++result.hits;
    ++set.hits;
  }
  else
  {
    access.outcome = LruAccessOutcome::ReplacementMiss;
    ++result.replacement_misses;
    ++set.replacement_misses;
  }
}

void analyze_set(const std::vector<IndexedLine> & lines,
                 std::uint64_t associativity, LruRdAnalysis & result,
                 LruSetResult & set)
{
  FenwickTree positions(lines.size());
  std::unordered_map<std::uint64_t, std::size_t> last_seen;
  for (std::size_t index = 0; index < lines.size(); ++index)
  {
    const auto position = index + 1;
    const auto & line = lines[index];
    const auto previous = last_seen.find(line.tag);
    if (previous == last_seen.end())
    {
      result.accesses[line.input_index].outcome = LruAccessOutcome::ColdMiss;
      ++result.cold_misses;
      ++set.cold_misses;
    }
    else
    {
      const auto newer = positions.prefix_sum(position - 1) -
                         positions.prefix_sum(previous->second);
      record_reuse(result, set, line.input_index,
                   static_cast<std::size_t>(newer), associativity);
      positions.add(previous->second, -1);
    }
    positions.add(position, 1);
    last_seen[line.tag] = position;
  }
}

}  // namespace

LruRdAnalysis analyze_lru_reuse(const std::vector<CacheLineMapping> & accesses,
                                const CacheGeometry & geometry)
{
  const auto set_count = cache_set_count(geometry);
  LruRdAnalysis result;
  result.accesses.resize(accesses.size());

  std::map<std::uint64_t, std::vector<IndexedLine>> lines_by_set;
  for (std::size_t index = 0; index < accesses.size(); ++index)
  {
    const auto & line = accesses[index].decoded;
    if (line.set_index >= set_count)
    {
      throw std::invalid_argument("cache access set index is out of range");
    }
    lines_by_set[line.set_index].push_back({index, line.tag});
  }

  for (const auto & [set_index, lines] : lines_by_set)
  {
    analyze_set(lines, geometry.associativity, result, result.sets[set_index]);
  }
  return result;
}

}  // namespace yarda
