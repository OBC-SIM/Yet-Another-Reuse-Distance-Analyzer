#include "exact_csrd_test_support.hpp"
#include "hierarchy_lru_oracle_differential_support.hpp"
#include "hierarchy_lru_oracle_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::support;

TEST(ExactCsrdDifferentialTest, MatchesNaiveAndBatchAtBothSeededGeometries)
{
  for (const auto & spec : seeded_oracle_trace_specs())
  {
    const auto trace = make_seeded_oracle_trace(spec);
    SCOPED_TRACE(describe_seeded_oracle_trace(spec, trace));
    expect_incremental_parity(trace.accesses, spec.l1_geometry);
    auto remapped = trace.accesses;
    for (auto & row : remapped)
      row.decoded =
        decode_cache_address(row.decoded.address, spec.llc_geometry);
    SCOPED_TRACE("LLC geometry");
    expect_incremental_parity(remapped, spec.llc_geometry);
  }
}

TEST(ExactCsrdDifferentialTest, MatchesOraclesWhileDistinctHistoryGrows)
{
  const CacheGeometry geometry{64, 4, 4};
  std::vector<CacheLineMapping> mappings;
  for (std::uint64_t block = 0; block < 64; ++block)
  {
    for (const auto key : {block, std::uint64_t{0}, block / 2, block})
      mappings.push_back(make_mapping(key, geometry, mappings.size()));
  }
  expect_incremental_parity(mappings, geometry);
}

TEST(ExactCsrdDifferentialTest, MatchesEmptyOracleAndBatchResults)
{
  expect_incremental_parity({}, {32, 4, 2});
}

class ExactCsrdFixedDomainTest : public testing::TestWithParam<std::uint64_t>
{
};

TEST_P(ExactCsrdFixedDomainTest, RetainedStateDoesNotGrowWithReferenceCount)
{
  const auto domain = GetParam();
  const CacheGeometry geometry{32, 2, 2};
  ExactCsrdAnalyzer analyzer(geometry);
  for (std::uint64_t block = 0; block < domain; ++block)
    observe_block(analyzer, geometry, block);
  const auto & index = detail::ExactCsrdTestAccess::index(analyzer, 0);
  const auto initial = detail::RecencyIndexTestAccess::storage(index);
  constexpr std::uint64_t references = 20000;
  for (auto i = domain; i < references; ++i)
  {
    const auto observation = observe_block(analyzer, geometry, i % domain);
    ASSERT_EQ(observation.distance, domain - 1);
    ASSERT_EQ(observation.outcome, LruAccessOutcome::Hit);
  }
  const auto final = detail::RecencyIndexTestAccess::storage(index);
  EXPECT_EQ(final.active, domain);
  EXPECT_EQ(final.allocated_elements, initial.allocated_elements);
  EXPECT_EQ(final.hash_buckets, initial.hash_buckets);
  EXPECT_LE(final.capacity, std::max<std::uint64_t>(16, 2 * domain));
  detail::RecencyIndexTestAccess::expect_invariants(index);
  expect_csrd_summary(analyzer.summary(),
                      {references,
                       references - domain,
                       domain,
                       0,
                       domain,
                       {{domain - 1, references - domain}}});
}

INSTANTIATE_TEST_SUITE_P(Lines, ExactCsrdFixedDomainTest,
                         testing::Values(1U, 2U));

}  // namespace
