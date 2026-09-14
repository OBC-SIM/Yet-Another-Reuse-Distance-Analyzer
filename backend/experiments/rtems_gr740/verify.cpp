#include "expectations.hpp"
#include "driver/evaluation.hpp"
#include "cache/hierarchy/hierarchy_analysis_oracle_support.hpp"
#include "yarda/elf/data_regions.hpp"
#include "yarda/trace/task_access_stream.hpp"

#include <iostream>
#include <tuple>

namespace
{
using namespace yarda;
std::string case_path, cli_result_path;

auto source_key(const ResolvedAccess & source)
{
  return std::tie(source.object_id, source.object_byte_offset, source.access_size,
    source.linked_byte_address, source.address_basis, source.operation,
    source.source_access_ordinal);
}

class RtemsGr740 : public testing::Test
{
protected:
  nlohmann::json row;
  evaluation::Inputs inputs;
  ResolvedTaskTraceResult expected;
  void SetUp() override
  {
    row = evaluation::read_json(case_path);
    inputs = evaluation::read_inputs(row);
    expected = rtems_experiment::expected_sources(row);
  }
};

TEST_F(RtemsGr740, SparcElfAndEverySourceMatchIndependentKernelEquations)
{
  const auto image = parse_elf_data_regions(row.at("elf_path").get<std::string>());
  ASSERT_EQ(image.address_size, 4);
  ASSERT_EQ(image.machine, 2); // EM_SPARC.
  ASSERT_EQ(image.endianness, ElfEndianness::Big);
  ASSERT_EQ(image.image_type, ElfImageType::Executable);
  std::uint64_t seen = 0, tasks = 0;
  const auto & task = expected.tasks.at(0);
  const TaskAccessSink sink{
    [&](const std::string & id, std::uint64_t excluded) {
      if (++tasks != 1 || id != task.task_id || excluded != 0)
        throw std::logic_error("unexpected task selection");
    },
    [&](const std::string &, const ResolvedAccess & actual) {
      if (seen >= task.accesses.size() || source_key(actual) != source_key(task.accesses[seen]))
        throw std::logic_error("independent source mismatch at " + std::to_string(seen));
      ++seen;
    },
    [](const std::string &, const TraceCoverage &) {}};
  const auto options = evaluation::work_options(row);
  TraceEmissionBudget budget(options.emission_limits);
  stream_resolved_task_accesses(inputs.raw, inputs.objects, sink, budget,
                               options.loop_limits);
  EXPECT_EQ(tasks, 1);
  EXPECT_EQ(seen, task.accesses.size());
}

TEST_F(RtemsGr740, EveryFirstHitMatchesIndependentResidentLru)
{
  const auto & hierarchy = inputs.metadata.hierarchy;
  const auto mapped = map_resolved_task_traces(expected, hierarchy.l1.geometry);
  const auto oracle = test::analyze_with_explicit_lru(mapped.tasks.at(0),
    hierarchy.l1.geometry, hierarchy.llc.geometry);
  auto options = evaluation::work_options(row);
  options.event_limit = oracle.events.size();
  std::size_t seen = 0;
  options.event_sink = [&](const std::string &, const HierarchyAccessEvent & event) {
    if (seen >= oracle.events.size()) throw std::logic_error("extra event");
    const auto & wanted = oracle.events.at(seen++);
    const auto actual_llc = event.llc
      ? std::optional<LruAccessOutcome>(event.llc->outcome) : std::nullopt;
    const auto first = wanted.first_service == test::OracleFirstServiceLevel::L1
      ? FirstServiceLevel::L1 : wanted.first_service == test::OracleFirstServiceLevel::LLC
      ? FirstServiceLevel::LLC : FirstServiceLevel::Memory;
    if (event.l1.outcome != wanted.l1_outcome || actual_llc != wanted.llc_outcome ||
        event.first_service != first)
      throw std::logic_error("independent resident-LRU mismatch");
  };
  const auto actual = analyze_streaming_hierarchy(inputs.raw, inputs.objects, hierarchy, options);
  ASSERT_EQ(actual.tasks.size(), 1);
  EXPECT_EQ(seen, oracle.events.size());
  EXPECT_EQ(actual.tasks[0].l1_first_hit_count, oracle.l1_first_hit_count);
  EXPECT_EQ(actual.tasks[0].llc_first_hit_count, oracle.llc_first_hit_count);
  EXPECT_EQ(actual.tasks[0].all_cache_misses, oracle.all_cache_misses);
  EXPECT_TRUE(actual.tasks[0].invariants.all_passed);
}

TEST_F(RtemsGr740, AllBatchStreamingEventsAndActualCliResultAgree)
{
  const auto options = evaluation::work_options(row);
  evaluation::verify_paths(inputs, options);
  const auto batch = analyze_batch_hierarchy(expected, inputs.metadata.hierarchy);
  EXPECT_EQ(evaluation::read_json(cli_result_path), nlohmann::json(hierarchy_result_json(
    inputs.metadata, evaluation::batch_summary(batch))));
}

TEST_F(RtemsGr740, MicroTraceMatchesNaiveExactDistanceOracle)
{
  if (row.at("expected_sources").get<std::uint64_t>() > 1000)
    GTEST_SKIP() << "quadratic oracle is restricted to micro datasets";
  test::support::expect_batch_matches_oracles(expected, inputs.metadata.hierarchy);
}
}

/**
 * @brief Verify a prepared RTEMS case against independent source expectations.
 * @param argc Program plus case and RESULT paths, optionally GTest flags.
 * @param argv Borrowed non-null command arguments.
 * @return Google Test result, or 2 for invalid arguments.
 */
int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  if (argc != 3)
  {
    std::cerr << "usage: yarda_rtems_gr740_verify CASE.json CLI_RESULT.json\n";
    return 2;
  }
  case_path = argv[1];
  cli_result_path = argv[2];
  return RUN_ALL_TESTS();
}
