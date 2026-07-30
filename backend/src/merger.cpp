#include "yarda/merger.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_set>

namespace yarda
{

const ReuseProfile &
BlockMerger::merge(const ReuseProfile & block_profile,
                   const std::vector<std::string> & block_trace)
{
  for (const auto & [distance, frequency] : block_profile.histogram)
  {
    profile_.histogram[distance] += frequency;
  }
  profile_.cold_misses.insert(block_profile.cold_misses.begin(),
                              block_profile.cold_misses.end());

  std::unordered_set<std::string> prior(lru_stack_.begin(), lru_stack_.end());
  for (const auto & address : block_trace)
  {
    const auto found = std::find(lru_stack_.begin(), lru_stack_.end(), address);
    if (found != lru_stack_.end())
    {
      const auto distance =
        static_cast<std::size_t>(std::distance(lru_stack_.begin(), found));
      if (prior.erase(address) > 0)
      {
        ++profile_.histogram[distance];
      }
      lru_stack_.erase(found);
    }
    lru_stack_.insert(lru_stack_.begin(), address);
  }
  return profile_;
}

const ReuseProfile & BlockMerger::profile() const { return profile_; }

}  // namespace yarda
