#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;

TEST(StreamingHierarchyTest, ClassifiesAllThreeServicesWithExactDistances)
{
  EventCollector collector;
  const auto result = analyze_streaming_hierarchy(
    byte_trace({0, 32, 64, 0, 0}), byte_addresses(),
    make_batch_hierarchy({32, 2, 2}, {32, 4, 2}), collector.options());
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  EXPECT_EQ(task.task_id, "kernel");
  stream::expect_coverage(task.coverage, (TraceCoverage{5, 5, 0, 5}));
  stream::expect_coverage(result.coverage, task.coverage);
  EXPECT_EQ(task.source_accesses, 5U);
  EXPECT_EQ(task.modeled_accesses, 5U);
  expect_level(task.l1,
               (CacheLevelSummary{5, 1, 4, 3, 1, 3, {{0, 1}, {2, 1}}}));
  expect_level(task.llc, (CacheLevelSummary{4, 1, 3, 3, 0, 3, {{1, 1}}}));
  EXPECT_EQ(task.ehc_l1, 1U);
  EXPECT_EQ(task.ehc_llc, 1U);
  EXPECT_EQ(task.all_cache_misses, 3U);
  EXPECT_EQ(task.hr_l1, 0.2);
  EXPECT_EQ(task.hr_llc, 0.2);
  EXPECT_EQ(task.miss_ratio, 0.6);
  EXPECT_TRUE(task.invariants.all_passed);
  ASSERT_EQ(collector.events.size(), 5U);
  const auto & reused = collector.events[3].second;
  EXPECT_EQ(reused.first_service, FirstServiceLevel::LLC);
  EXPECT_EQ(reused.l1.reuse_distance, 2U);
  ASSERT_TRUE(reused.llc);
  EXPECT_EQ(reused.llc->reuse_distance, 1U);
  EXPECT_EQ(collector.events[4].second.first_service, FirstServiceLevel::L1);
}

TEST(StreamingHierarchyTest, PreservesTaskOrderEmptyTasksAndColdHistories)
{
  const auto raw = byte_module(Json::array({
    function("zeta", byte_body({0, 0})),
    function("empty", Json::array()),
    function("alpha", byte_body({0, 0})),
  }));
  EventCollector collector;
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy(), collector.options());
  ASSERT_EQ(result.tasks.size(), 3U);
  EXPECT_EQ(result.tasks[0].task_id, "zeta");
  EXPECT_EQ(result.tasks[1].task_id, "empty");
  EXPECT_EQ(result.tasks[2].task_id, "alpha");
  stream::expect_coverage(result.coverage, (TraceCoverage{4, 4, 0, 4}));
  for (const auto index : {0U, 2U})
  {
    const auto & task = result.tasks[index];
    stream::expect_coverage(task.coverage, (TraceCoverage{2, 2, 0, 2}));
    expect_level(task.l1, (CacheLevelSummary{2, 1, 1, 1, 0, 1, {{0, 1}}}));
    expect_level(task.llc, (CacheLevelSummary{1, 0, 1, 1, 0, 1, {}}));
    EXPECT_EQ(task.ehc_l1, 1U);
    EXPECT_EQ(task.all_cache_misses, 1U);
    EXPECT_TRUE(task.invariants.all_passed);
  }
  const auto & empty = result.tasks[1];
  stream::expect_coverage(empty.coverage, {});
  expect_level(empty.l1, {});
  expect_level(empty.llc, {});
  EXPECT_EQ(empty.modeled_accesses, 0U);
  EXPECT_FALSE(empty.hr_l1);
  EXPECT_FALSE(empty.hr_llc);
  EXPECT_FALSE(empty.miss_ratio);
  EXPECT_TRUE(empty.invariants.all_passed);
  ASSERT_EQ(collector.events.size(), 4U);
  EXPECT_EQ(collector.events[2].first, "alpha");
  EXPECT_EQ(collector.events[2].second.l1_mapping.source_access_ordinal, 0U);
  EXPECT_EQ(collector.events[2].second.first_service,
            FirstServiceLevel::Memory);
}

TEST(StreamingHierarchyTest, CountsCrossLineReferencesSeparatelyFromSources)
{
  const auto raw = stream::module(
    Json::array({function(
      "wide", Json::array({access(), access("global::A", "0", "store")}))}),
    40);
  EventCollector collector;
  const auto result = analyze_streaming_hierarchy(
    raw, stream::addresses(40), make_batch_hierarchy(), collector.options());
  ASSERT_EQ(result.tasks.size(), 1U);
  stream::expect_coverage(result.coverage, (TraceCoverage{2, 2, 0, 6}));
  stream::expect_coverage(result.tasks[0].coverage, result.coverage);
  EXPECT_EQ(result.tasks[0].modeled_accesses, 6U);
  EXPECT_EQ(result.tasks[0].ehc_l1, 3U);
  EXPECT_EQ(result.tasks[0].all_cache_misses, 3U);
  ASSERT_EQ(collector.events.size(), 6U);
  const std::uint64_t offsets[] = {0, 4, 36};
  for (std::size_t i = 0; i < collector.events.size(); ++i)
  {
    const auto & row = collector.events[i].second.l1_mapping;
    EXPECT_EQ(row.object_id, "global::A");
    EXPECT_EQ(row.object_byte_offset, offsets[i % 3]);
    EXPECT_EQ(row.decoded.address, 0x101c + offsets[i % 3]);
    EXPECT_EQ(row.source_linked_byte_address, 0x101cU);
    EXPECT_EQ(row.source_object_byte_offset, 0U);
    EXPECT_EQ(row.source_access_size, 40U);
    EXPECT_EQ(row.source_access_ordinal, i / 3);
    EXPECT_EQ(row.line_span_ordinal, i % 3);
    EXPECT_EQ(row.operation,
              i < 3 ? AccessOperation::Load : AccessOperation::Store);
  }
}

TEST(StreamingHierarchyTest, CountsAliasedObjectsAsOneHistoricalLine)
{
  const auto raw = byte_module(
    Json::array({function("kernel", Json::array({
                                      access(),
                                      access("global::B", "7", "store"),
                                      access("global::A", "3"),
                                    }))}));
  auto objects = byte_addresses();
  objects.objects.at("global::B").base = 0;
  const auto result =
    analyze_streaming_hierarchy(raw, objects, make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 1U);
  expect_level(result.tasks[0].l1,
               (CacheLevelSummary{3, 2, 1, 1, 0, 1, {{0, 2}}}));
  EXPECT_EQ(result.tasks[0].llc.lookups, 1U);
}

TEST(StreamingHierarchyTest, ExcludesOtherSetsFromReuseDistance)
{
  EventCollector collector;
  const auto result = analyze_streaming_hierarchy(
    byte_trace({0, 32, 64, 96, 0}), byte_addresses(),
    make_batch_hierarchy({32, 4, 1}, {32, 8, 2}), collector.options());
  ASSERT_EQ(collector.events.size(), 5U);
  EXPECT_EQ(collector.events.back().second.l1.reuse_distance, 0U);
  EXPECT_EQ(collector.events.back().second.first_service,
            FirstServiceLevel::L1);
  EXPECT_EQ(result.tasks.at(0).llc.lookups, 4U);
}

TEST(StreamingHierarchyTest, RetainsDistancesFarBeyondAssociativity)
{
  auto body = Json::array();
  for (unsigned i = 0; i < 18; ++i)
    body.push_back(access("global::A", std::to_string(i * 32)));
  body.push_back(access());
  const auto raw = byte_module(Json::array({function("kernel", body)}));
  EventCollector collector;
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy({32, 2, 2}, {32, 2, 2}),
    collector.options());
  ASSERT_EQ(collector.events.size(), 19U);
  const auto & last = collector.events.back().second;
  EXPECT_EQ(last.l1.reuse_distance, 17U);
  ASSERT_TRUE(last.llc);
  EXPECT_EQ(last.llc->reuse_distance, 17U);
  EXPECT_EQ(last.first_service, FirstServiceLevel::Memory);
  expect_level(result.tasks.at(0).l1,
               (CacheLevelSummary{19, 0, 19, 18, 1, 18, {{17, 1}}}));
  expect_level(result.tasks.at(0).llc, result.tasks.at(0).l1);
}

TEST(StreamingHierarchyTest, ProcessesCompactRepeatedLoopsInSummaryMode)
{
  const auto raw = byte_module(Json::array(
    {function("repeat", Json::array({loop(20000, byte_body({0, 32}))}))}));
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy({32, 1, 1}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  stream::expect_coverage(result.coverage,
                          (TraceCoverage{40000, 40000, 0, 40000}));
  expect_level(task.l1,
               (CacheLevelSummary{40000, 0, 40000, 2, 39998, 2, {{1, 39998}}}));
  expect_level(task.llc,
               (CacheLevelSummary{40000, 39998, 2, 2, 0, 2, {{1, 39998}}}));
  EXPECT_EQ(task.ehc_llc, 39998U);
  EXPECT_EQ(task.all_cache_misses, 2U);
  EXPECT_EQ(result.event_delivery.emitted_events, 0U);
  EXPECT_FALSE(result.event_delivery.events_truncated);
  EXPECT_TRUE(task.invariants.all_passed);
}

}  // namespace
