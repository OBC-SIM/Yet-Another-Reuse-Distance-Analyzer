#include <gtest/gtest.h>
#include <vector>

#include "yarda/cache/lru_rd_analysis.hpp"
#include "yarda/trace/mapped_trace.hpp"

namespace
{

TEST(LruRdIntegrationTest, PreservesLruStateAcrossMappedBlocks)
{
  const yarda::CacheGeometry geometry{64, 1, 1};

  const yarda::CacheLineMapping line = {
    "global::A",
    0,
    yarda::AddressBasis::Absolute,
    yarda::decode_cache_address(0x1000, geometry),
  };

  const std::vector<yarda::NamedMappedTrace> blocks = {
    {"kernel i-loop", {line}},
    {"kernel j-loop", {line}},
  };

  const auto accesses = yarda::flatten_mapped_traces(blocks);
  const auto result = yarda::analyze_lru_reuse(accesses, geometry);

  ASSERT_EQ(accesses.size(), 2);
  ASSERT_EQ(result.accesses.size(), 2);

  EXPECT_EQ(result.accesses[0].outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_FALSE(result.accesses[0].reuse_distance.has_value());

  EXPECT_EQ(result.accesses[1].outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.accesses[1].reuse_distance, 0U);

  EXPECT_EQ(result.cold_misses, 1U);
  EXPECT_EQ(result.hits, 1U);
  EXPECT_EQ(result.replacement_misses, 0U);
}

}  // namespace
