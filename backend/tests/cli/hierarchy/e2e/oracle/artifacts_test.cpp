#include "fixture.hpp"

#include "cache/hierarchy/hierarchy_analysis_oracle_support.hpp"
#include "trace/output/task_access_stream_test_support.hpp"

namespace yarda::test::e2e
{
TEST_F(GeneratedArtifacts, GeneratedSourceOrderMatchesIndependentExpectedAddresses)
{
  const auto actual = resolved_task_traces(raw, objects);
  ASSERT_EQ(actual.tasks.size(), expected.tasks.size());
  for (std::size_t task = 0; task < expected.tasks.size(); ++task)
  {
    SCOPED_TRACE(expected.tasks[task].task_id);
    EXPECT_EQ(actual.tasks[task].task_id, expected.tasks[task].task_id);
    ASSERT_EQ(actual.tasks[task].accesses.size(), expected.tasks[task].accesses.size());
    for (std::size_t source = 0; source < expected.tasks[task].accesses.size(); ++source)
    {
      SCOPED_TRACE(source);
      stream::expect_access(actual.tasks[task].accesses[source],
                            expected.tasks[task].accesses[source]);
    }
  }
}

TEST_F(GeneratedArtifacts, IndependentSourcesMatchBothCacheOracles)
{
  support::expect_batch_matches_oracles(expected, metadata.hierarchy);
}

TEST_F(GeneratedArtifacts, PublishedBytesMatchBatchOnIndependentSources)
{
  const auto batch = analyze_batch_hierarchy(expected, metadata.hierarchy);
  StreamingHierarchyResult summaries;
  summaries.coverage = batch.coverage;
  std::vector<HierarchyEventRecord> records;
  for (const auto & task : batch.tasks)
  {
    summaries.tasks.push_back(task.summary);
    for (const auto & event : task.events)
      records.push_back({task.summary.task_id, event});
  }
  const auto count = static_cast<std::uint64_t>(records.size());
  const auto limit = events.at("event_limit").get<std::uint64_t>();
  ASSERT_GE(limit, count) << "oracle verification requires complete diagnostics";
  const HierarchyEventMetadata event_metadata{
    hierarchy_analysis_id(metadata.identity), limit, count, {count, false}};
  EXPECT_EQ(result_bytes, hierarchy_result_json(metadata, summaries).dump(2) + "\n");
  EXPECT_EQ(event_bytes, hierarchy_events_json(event_metadata, records).dump(2) + "\n");
}
} // namespace yarda::test::e2e
