#include "test_support.hpp"
#include "build_version.hpp"

namespace
{
using namespace yarda::test::cli;
using yarda::cli::run_hierarchy_command;

TEST_F(HierarchyCommandTest, BindsBuildIdentityAndPreservesColdTasks)
{
  run_hierarchy_command(options);
  const auto result = Json::parse(read(options.export_path));
  EXPECT_EQ(result["tool_version"], yarda::cli::kToolVersion);
  ASSERT_EQ(result["tasks"].size(), 3U);
  EXPECT_EQ(result["tasks"][0]["all_cache_miss_count"], 2);
  EXPECT_EQ(result["tasks"][1]["all_cache_miss_count"], 2);
  EXPECT_EQ(result["tasks"][2]["l1_first_hit_ratio"], nullptr);
  EXPECT_FALSE(fs::exists(temporary.directory / "events.json"));
  EXPECT_FALSE(fs::exists(temporary.directory / "telemetry.json"));
}

TEST_F(HierarchyCommandTest, StartsMeasurementsBeforeReadingInputs)
{
  diagnostics();
  fs::remove(options.input);
  Measurements clock;
  auto providers = clock.providers();
  providers.now_ns = [&]
  {
    if (clock.ticks == 0) write(options.input, raw().dump());
    clock.ticks += 10;
    return clock.ticks;
  };
  providers.peak_rss_bytes = [&]
  {
    expect_no_outputs();
    return 4096;
  };
  run_hierarchy_command(options, &providers);
  const auto telemetry = Json::parse(read(options.telemetry_path));
  EXPECT_EQ(telemetry["peak_rss_bytes"], 4096);
  EXPECT_EQ(telemetry["source_accesses_emitted"], 4);
  EXPECT_EQ(telemetry["line_references_emitted"], 8);
  EXPECT_EQ(telemetry["loop_iterations_expanded"], 4);
  for (const auto & stage : telemetry["stage_time_ns"])
    EXPECT_TRUE(stage.is_number_unsigned());
}

TEST_F(HierarchyCommandTest, DisabledTelemetryNeverInvokesProviders)
{
  Measurements clock;
  auto providers = clock.providers();
  run_hierarchy_command(options, &providers);
  EXPECT_EQ(clock.ticks, 0U);
}

TEST_F(HierarchyCommandTest, DiscardsCompletedTaskEventsAfterLaterTaskFailure)
{
  diagnostics();
  options.emission_limits.emitted_source_accesses = 3;
  EXPECT_THROW(run_hierarchy_command(options), std::invalid_argument);
  expect_no_outputs();
  options.emission_limits.emitted_source_accesses = 4;
  run_hierarchy_command(options);
  const auto events = Json::parse(read(options.events_path));
  ASSERT_EQ(events["events"].size(), 8U);
  EXPECT_EQ(events["events"][0]["task_id"], "first");
  EXPECT_EQ(events["events"][4]["task_id"], "second");
}

TEST_F(HierarchyCommandTest, OptionalDumpFailurePreservesExistingResult)
{
  diagnostics();
  write(options.export_path, "previous result");
  Measurements clock;
  auto providers = clock.providers();
  providers.host = []
  {
    return AnalysisTelemetryHost{std::string(1, '\xff'), "Linux", "test",
                                 "x86_64"};
  };
  EXPECT_THROW(run_hierarchy_command(options, &providers), Json::type_error);
  EXPECT_EQ(read(options.export_path), "previous result");
  EXPECT_FALSE(fs::exists(options.events_path));
  EXPECT_FALSE(fs::exists(options.telemetry_path));
}

TEST_F(HierarchyCommandTest, ProviderFailurePublishesNoArtifacts)
{
  diagnostics();
  Measurements clock;
  auto providers = clock.providers();
  providers.peak_rss_bytes = []() -> std::uint64_t
  { throw std::runtime_error("injected measurement failure"); };
  EXPECT_THROW(run_hierarchy_command(options, &providers), std::runtime_error);
  expect_no_outputs();
}

TEST_F(HierarchyCommandTest, ResultDumpFailurePrecedesSnapshotAndPublication)
{
  diagnostics();
  auto cache = read(options.cache_path);
  for (auto at = cache.find("L1D0"); at != std::string::npos;
       at = cache.find("L1D0"))
    cache.replace(at, 4, std::string("L1D") + '\xff');
  write(options.cache_path, cache);
  Measurements clock;
  auto providers = clock.providers();
  bool snapshot_taken = false;
  providers.peak_rss_bytes = [&]
  {
    snapshot_taken = true;
    return 4096;
  };
  EXPECT_THROW(run_hierarchy_command(options, &providers), Json::type_error);
  EXPECT_FALSE(snapshot_taken);
  expect_no_outputs();
}

TEST_F(HierarchyCommandTest,
       RejectsUnversionedMapInsteadOfInventingInputVersion)
{
  auto input = raw();
  input.erase("schema_version");
  write(options.input, input.dump());
  EXPECT_THROW(run_hierarchy_command(options), std::invalid_argument);
  expect_no_outputs();
}

TEST_F(HierarchyCommandTest, RetainsRegionTaskIdentityInResultAndEvents)
{
  auto input = raw();
  input["functions"][0]["analysis_scope"] = {{"kind", "region"},
                                             {"name", "APE_ANALYZE"}};
  write(options.input, input.dump());
  diagnostics(1);
  run_hierarchy_command(options);
  const auto result = Json::parse(read(options.export_path));
  const auto events = Json::parse(read(options.events_path));
  EXPECT_EQ(result["tasks"][0]["task_id"], "region:5:first:APE_ANALYZE");
  EXPECT_EQ(events["events"][0]["task_id"], result["tasks"][0]["task_id"]);
}

TEST_F(HierarchyCommandTest, AllowsZeroBudgetsForAnEmptyTask)
{
  auto input = raw();
  input["functions"] = Json::array({function("empty", Json::array())});
  write(options.input, input.dump());
  options.loop_limits = {0, 0};
  options.emission_limits = {0, 0};
  diagnostics(0);
  run_hierarchy_command(options);
  const auto events = Json::parse(read(options.events_path));
  EXPECT_TRUE(events["events"].empty());
  EXPECT_EQ(events["events_truncated"], false);
}
} // namespace
