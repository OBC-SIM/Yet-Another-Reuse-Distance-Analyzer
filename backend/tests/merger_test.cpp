#include "yarda/merger.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{

TEST(BlockMergerTest, AddsCrossBlockFirstReuse)
{
  yarda::BlockMerger merger;
  yarda::ReuseProfile first;
  first.cold_misses = {"A", "B"};
  merger.merge(first, {"A", "B"});

  yarda::ReuseProfile second;
  second.cold_misses = {"A"};
  const auto & merged = merger.merge(second, {"A"});

  EXPECT_EQ(merged.histogram, (std::map<std::size_t, std::uint64_t>{{1, 1}}));
  EXPECT_EQ(merged.cold_misses, (std::unordered_set<std::string>{"A", "B"}));
}

TEST(BlockMergerTest, PreservesIntraBlockHistogram)
{
  yarda::BlockMerger merger;
  yarda::ReuseProfile block;
  block.histogram = {{0, 4}};

  const auto & merged = merger.merge(block, {"A", "A"});

  EXPECT_EQ(merged.histogram.at(0), 4);
}

}  // namespace
