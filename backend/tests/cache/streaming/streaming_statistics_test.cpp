#include "streaming_hierarchy_test_support.hpp"
#include "cache/output/artifact_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::streaming;

TEST(StreamingStatisticsTest, ReportsSeparateColdTaskHistories)
{
  const auto raw = byte_module(Json::array({
    function("first", byte_body({0, 32, 0})),
    function("second", byte_body({0})), function("empty", Json::array())}));
  std::vector<std::string> ids;
  std::vector<std::uint64_t> entries;
  StreamingHierarchyOptions options;
  options.statistics_sink = [&](const std::string & id,
                                 const CsrdStatistics & l1,
                                 const CsrdStatistics & llc) {
    ids.push_back(id);
    entries.push_back(l1.active_history_entries);
    EXPECT_EQ(llc.active_history_entries, l1.active_history_entries);
  };
  const auto meta = yarda::test::artifact::metadata();
  const auto actual = analyze_streaming_hierarchy(raw, byte_addresses(),
                                                  meta.hierarchy, options);
  const auto plain = analyze_streaming_hierarchy(raw, byte_addresses(),
                                                 meta.hierarchy);
  ASSERT_EQ(ids.size(), 3U);
  EXPECT_EQ(entries, (std::vector<std::uint64_t>{2, 1, 0}));
  for (std::size_t i = 0; i < ids.size(); ++i)
    EXPECT_EQ(ids[i], actual.tasks[i].task_id);
  EXPECT_EQ(hierarchy_result_json(meta, actual).dump(),
            hierarchy_result_json(meta, plain).dump());
  EXPECT_EQ(actual.event_delivery.emitted_events, 0U);
}

TEST(StreamingStatisticsTest, DoesNotReportAnInterruptedTask)
{
  std::size_t callbacks = 0;
  StreamingHierarchyOptions options;
  options.emission_limits.emitted_source_accesses = 1;
  options.statistics_sink = [&](const auto &, const auto &, const auto &) {
    ++callbacks;
  };
  EXPECT_THROW(analyze_streaming_hierarchy(byte_trace({0, 32}),
    byte_addresses(), make_batch_hierarchy(), options), std::invalid_argument);
  EXPECT_EQ(callbacks, 0U);
}

TEST(StreamingStatisticsTest, CallbackExceptionStopsLaterTasks)
{
  std::size_t callbacks = 0;
  StreamingHierarchyOptions options;
  options.statistics_sink = [&](const auto &, const auto &, const auto &) {
    ++callbacks;
    throw std::runtime_error("stop measurement");
  };
  const auto raw = byte_module(Json::array({function("first", byte_body({0})),
                                           function("next", byte_body({32}))}));
  EXPECT_THROW(analyze_streaming_hierarchy(raw, byte_addresses(),
    make_batch_hierarchy(), options), std::runtime_error);
  EXPECT_EQ(callbacks, 1U);
}
} // namespace
