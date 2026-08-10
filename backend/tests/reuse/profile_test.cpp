#include "yarda/reuse/profile.hpp"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{

yarda::ReuseProfile reference_profile(const std::vector<std::string> & trace)
{
  yarda::ReuseProfile profile;
  std::vector<std::string> stack;
  for (const auto & address : trace)
  {
    const auto found = std::find(stack.begin(), stack.end(), address);
    if (found == stack.end())
    {
      profile.cold_misses.insert(address);
    }
    else
    {
      const auto distance =
        static_cast<std::size_t>(std::distance(stack.begin(), found));
      ++profile.histogram[distance];
      stack.erase(found);
    }
    stack.insert(stack.begin(), address);
  }
  return profile;
}

TEST(ReuseProfileTest, CalculatesKnownDistances)
{
  const auto profile = yarda::calculate_reuse_profile({"A", "B", "A", "A"});

  EXPECT_EQ(profile.histogram,
            (std::map<std::size_t, std::uint64_t>{{0, 1}, {1, 1}}));
  EXPECT_EQ(profile.cold_misses, (std::unordered_set<std::string>{"A", "B"}));
}

TEST(ReuseProfileTest, MatchesListStackReference)
{
  std::vector<std::string> trace;
  for (int index = 0; index < 10'000; ++index)
  {
    trace.push_back("line-" + std::to_string((index * 7) % 97));
  }
  const auto expected = reference_profile(trace);
  const auto actual = yarda::calculate_reuse_profile(trace);

  EXPECT_EQ(actual.histogram, expected.histogram);
  EXPECT_EQ(actual.cold_misses, expected.cold_misses);
}

TEST(ReuseProfileTest, HandlesEmptyTrace)
{
  const auto profile = yarda::calculate_reuse_profile({});

  EXPECT_TRUE(profile.histogram.empty());
  EXPECT_TRUE(profile.cold_misses.empty());
}

}  // namespace
