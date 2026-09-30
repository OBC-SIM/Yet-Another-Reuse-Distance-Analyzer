#include "task_access_stream_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::stream;

TEST(TaskAccessStreamTest, PreservesTaskOrderEmptyTasksAndLocalOrdinals)
{
  const auto raw = module(Json::array({
    function("first",
             Json::array({access(), access("global::B", "1", "store")})),
    function("empty", Json::array()),
    function("last", Json::array({access()}), "yard.analyze"),
  }));
  const auto batch = resolved_task_traces(raw, addresses());
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));

  expect_tasks(collector.result, batch);
  EXPECT_EQ(
    collector.notifications,
    (std::vector<std::string>{"begin:first", "access:first:0", "access:first:1",
                              "end:first", "begin:empty", "end:empty",
                              "begin:last", "access:last:0", "end:last"}));
  ASSERT_EQ(collector.result.tasks.size(), 3U);
  expect_coverage(collector.result.tasks[0].coverage, {2, 2, 0, 0});
  expect_coverage(collector.result.tasks[1].coverage, {});
  expect_coverage(collector.result.tasks[2].coverage, {1, 1, 0, 0});
  expect_coverage(collector.result.coverage, {3, 3, 0, 0});
  const auto & first = collector.result.tasks[0].accesses;
  ASSERT_EQ(first.size(), 2U);
  expect_access(first[0], {"global::A", 0, 4, 0x101c, AddressBasis::Absolute,
                           AccessOperation::Load, 0});
  expect_access(first[1], {"global::B", 4, 4, 0x2004, AddressBasis::Absolute,
                           AccessOperation::Store, 1});
  ASSERT_EQ(collector.result.tasks[2].accesses.size(), 1U);
  EXPECT_EQ(collector.result.tasks[2].accesses[0].source_access_ordinal, 0U);
}

TEST(TaskAccessStreamTest, PreservesNestedInlineAndLoopOrder)
{
  const auto raw = module(Json::array({
    function("inner", Json::array({access("global::C", "i", "store")}),
             "ape.inline"),
    function("outer", Json::array({access("global::B", "i"), call("inner")}),
             "yard.inline"),
    function("kernel",
             Json::array({access(), loop(2, Json::array({call("outer")})),
                          access("global::A", "1")})),
  }));
  const auto batch = resolved_task_traces(raw, addresses());
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));

  expect_tasks(collector.result, batch);
  ASSERT_EQ(collector.result.tasks.size(), 1U);
  const auto & values = collector.result.tasks[0].accesses;
  ASSERT_EQ(values.size(), 6U);
  const std::vector<std::uint64_t> expected{0x101c, 0x2000, 0x3000,
                                            0x2004, 0x3004, 0x1020};
  for (std::size_t index = 0; index < values.size(); ++index)
  {
    EXPECT_EQ(values[index].linked_byte_address, expected[index]);
    EXPECT_EQ(values[index].source_access_ordinal, index);
  }
  EXPECT_EQ(values[2].operation, AccessOperation::Store);
  EXPECT_EQ(values[4].operation, AccessOperation::Store);
}

TEST(TaskAccessStreamTest, PreservesDescendingLoopAndAffineIndex)
{
  const auto raw = module(Json::array({function(
    "kernel", Json::array({
                loop(-1, Json::array({access("global::A", "i+1")}), "i", 4, -2),
              }))}));
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));

  expect_tasks(collector.result, resolved_task_traces(raw, addresses()));
  const auto & values = collector.result.tasks.at(0).accesses;
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0].object_byte_offset, 20U);
  EXPECT_EQ(values[1].object_byte_offset, 12U);
  EXPECT_EQ(values[2].object_byte_offset, 4U);
}

TEST(TaskAccessStreamTest,
     ReportsStaticOpaqueSitesAtBeginWithoutLoopMultiplication)
{
  const auto body = Json::array({
    loop(3, Json::array({call("helper")})),
    loop(0, Json::array({call("opaque")})),
  });
  const auto raw = module(Json::array({
    function("opaque", Json::array(), ""),
    function("helper", Json::array({call("opaque"), access()}), "ape.inline"),
    function("first", body),
    function("second", body),
  }));
  Collector collector;
  auto sink = collector.sink();
  const auto record_begin = sink.begin_task;
  sink.begin_task = [&](const std::string & id, std::uint64_t excluded) {
    EXPECT_EQ(excluded, 2U);
    record_begin(id, excluded);
  };
  collector.complete(stream_resolved_task_accesses(raw, addresses(), sink));

  expect_tasks(collector.result, resolved_task_traces(raw, addresses()));
  ASSERT_EQ(collector.result.tasks.size(), 2U);
  EXPECT_EQ(collector.result.excluded_opaque_call_sites, 4U);
  expect_coverage(collector.result.coverage, {6, 6, 0, 0});
}

TEST(TaskAccessStreamTest, KeepsCalledAnalyzeRootOpaqueAndEmitsItIndependently)
{
  const auto raw = module(Json::array({
    function("caller", Json::array({call("callee")})),
    function("callee", Json::array({access()})),
  }));
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));

  expect_tasks(collector.result, resolved_task_traces(raw, addresses()));
  ASSERT_EQ(collector.result.tasks.size(), 2U);
  EXPECT_EQ(collector.result.tasks[0].excluded_opaque_call_sites, 1U);
  EXPECT_TRUE(collector.result.tasks[0].accesses.empty());
  EXPECT_EQ(collector.result.tasks[1].accesses.size(), 1U);
}

TEST(TaskAccessStreamTest,
     DeliversEarlierLoopAccessesBeforeLaterResolutionFailure)
{
  const auto raw = module(Json::array(
    {function("kernel", Json::array({
                          loop(3, Json::array({access("global::A", "i")})),
                        }))}));
  auto objects = addresses();
  objects.objects["global::A"].size = 8;
  Collector collector;

  EXPECT_THROW(stream_resolved_task_accesses(raw, objects, collector.sink()),
               ResolutionError);
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "access:kernel:0",
                                      "access:kernel:1"}));
}

TEST(TaskAccessStreamTest, CountsRepeatedReferencesWithoutCollectingTheirTrace)
{
  const auto raw = module(
    Json::array({function("kernel", Json::array({
                                      loop(200'000, Json::array({access()})),
                                    }))}));
  std::uint64_t count = 0;
  auto sink = discard_sink();
  sink.access = [&](const std::string &, const ResolvedAccess & value) {
    ASSERT_EQ(value.source_access_ordinal, count);
    ASSERT_EQ(value.linked_byte_address, 0x101cU);
    ++count;
  };
  const auto summary = stream_resolved_task_accesses(raw, addresses(), sink);

  EXPECT_EQ(count, 200'000U);
  expect_coverage(summary.coverage, {200'000, 200'000, 0, 0});
}

TEST(TaskAccessStreamTest,
     IndependentExecutionsStartWithFreshOrdinalsAndCoverage)
{
  const auto raw = module(Json::array({
    function("kernel", Json::array({access(), access()})),
  }));
  Collector first;
  Collector second;
  first.complete(stream_resolved_task_accesses(raw, addresses(), first.sink()));
  second.complete(
    stream_resolved_task_accesses(raw, addresses(), second.sink()));

  expect_tasks(first.result, second.result);
  expect_coverage(second.result.coverage, {2, 2, 0, 0});
  ASSERT_EQ(second.result.tasks.size(), 1U);
  ASSERT_EQ(second.result.tasks[0].accesses.size(), 2U);
  EXPECT_EQ(second.result.tasks[0].accesses[0].source_access_ordinal, 0U);
}

}  // namespace
