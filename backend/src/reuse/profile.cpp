#include "yarda/reuse/profile.hpp"

#include <unordered_map>
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

}  // namespace

ReuseProfile calculate_reuse_profile(const std::vector<std::string> & trace)
{
  ReuseProfile profile;
  FenwickTree positions(trace.size());
  std::unordered_map<std::string, std::size_t> last_seen;

  for (std::size_t index = 0; index < trace.size(); ++index)
  {
    const std::size_t position = index + 1;
    const auto & address = trace[index];
    const auto previous = last_seen.find(address);
    if (previous == last_seen.end())
    {
      profile.cold_misses.insert(address);
    }
    else
    {
      const auto newer = positions.prefix_sum(position - 1) -
                         positions.prefix_sum(previous->second);
      ++profile.histogram[static_cast<std::size_t>(newer)];
      positions.add(previous->second, -1);
    }
    positions.add(position, 1);
    last_seen[address] = position;
  }
  return profile;
}

}  // namespace yarda
