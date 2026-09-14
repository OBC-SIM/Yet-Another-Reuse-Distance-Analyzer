#include "prepared_trace_test_support.hpp"
#include "yarda/trace/trace.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::prepared;

TEST(PreparedTraceStreamTest, PreservesResolvedFieldsThroughVariableShadowing)
{
  const auto read = access("global::A", "i");
  const auto raw = module(Json::array({function(
    "kernel",
    Json::array({
      loop(3, Json::array({read, loop(12, Json::array({read}), "i", 10), read}),
           "i", 1),
    }))}));
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));
  ASSERT_EQ(collector.result.tasks.size(), 1U);
  expect_offsets(collector.result.tasks[0], {4, 40, 44, 4, 8, 40, 44, 8});
  expect_coverage(collector.result.coverage, {8, 8, 0, 0});
}

TEST(PreparedTraceStreamTest, ResetsOrdinalsAcrossDifferentSteppedTasks)
{
  const auto body = Json::array({access("global::A", "i")});
  const auto raw = module(Json::array({
    function("step2", Json::array({loop(7, body, "i", 1, 2)})),
    function("step32", Json::array({loop(64, body, "i", 0, 32)})),
    function("descending", Json::array({loop(0, body, "i", 5, -2)})),
  }));
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));
  ASSERT_EQ(collector.result.tasks.size(), 3U);
  expect_offsets(collector.result.tasks[0], {4, 12, 20});
  expect_offsets(collector.result.tasks[1], {0, 128});
  expect_offsets(collector.result.tasks[2], {20, 12, 4});
  expect_coverage(collector.result.coverage, {8, 8, 0, 0});
}

TEST(PreparedTraceStreamTest, KeepsInlineActualObjectsShapesAndSlotsSeparate)
{
  auto read = access("function:touch::param:x", "row", "store");
  read["indices"] = Json::array({"row", "j"});
  auto helper = function(
    "touch", Json::array({loop(2, Json::array({read}), "j")}), "ape.inline");
  helper["params"] = Json::array({"x", "row"});
  auto first = call("touch");
  first["args"] = Json::array({"A", "i"});
  first["arg_objects"] = Json::array({"global::A", ""});
  auto second = first;
  second["args"][0] = "B";
  second["arg_objects"][0] = "global::B";
  auto raw = module(Json::array(
    {helper, function("kernel", Json::array({
                                  loop(2, Json::array({first, second})),
                                }))}));
  raw["metadata"]["objects"]["global::A"]["shape"] = Json::array({2, 3});
  raw["metadata"]["objects"]["global::B"]["shape"] = Json::array({2, 4});
  raw["metadata"]["objects"]["global::B"]["elem_size"] = 8;
  const auto original = raw;
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));
  EXPECT_EQ(raw, original);
  ASSERT_EQ(collector.result.tasks.size(), 1U);
  const auto & actual = collector.result.tasks[0].accesses;
  ASSERT_EQ(actual.size(), 8U);
  std::size_t ordinal = 0;
  for (std::uint64_t i = 0; i < 2; ++i)
    for (const auto & object : {std::string("global::A"), std::string("global::"
                                                                      "B")})
      for (std::uint64_t j = 0; j < 2; ++j)
      {
        const auto width = object == "global::A" ? 4U : 8U;
        const auto columns = object == "global::A" ? 3U : 4U;
        const auto base = object == "global::A" ? 0x101cU : 0x2000U;
        const auto offset = (i * columns + j) * width;
        expect_access(actual[ordinal], {object, offset, width, base + offset,
                                        AddressBasis::Absolute,
                                        AccessOperation::Store, ordinal});
        ++ordinal;
      }
  expect_coverage(collector.result.coverage, {8, 8, 0, 0});
}

TEST(PreparedTraceStreamTest, ReentrantAnalysisCannotChangeOuterSlotsOrObjects)
{
  const auto raw = module(Json::array(
    {function("kernel", Json::array({
                          loop(3, Json::array({access("global::A", "i")})),
                        }))}));
  auto other = addresses();
  other.objects["global::A"].base = 0x9000;
  Collector outer;
  auto sink = outer.sink();
  const auto record = sink.access;
  sink.access = [&](const std::string & id, const ResolvedAccess & value) {
    record(id, value);
    if (value.source_access_ordinal != 0) return;
    const auto nested = resolved_task_traces(raw, other);
    ASSERT_EQ(nested.tasks.size(), 1U);
    ASSERT_EQ(nested.tasks[0].accesses.size(), 3U);
    for (std::size_t i = 0; i < 3; ++i)
      expect_access(nested.tasks[0].accesses[i],
                    {"global::A", 4 * i, 4, 0x9000 + 4 * i,
                     AddressBasis::Absolute, AccessOperation::Load, i});
  };
  outer.complete(stream_resolved_task_accesses(raw, addresses(), sink));
  ASSERT_EQ(outer.result.tasks.size(), 1U);
  expect_offsets(outer.result.tasks[0], {0, 4, 8});
}

TEST(PreparedTraceStreamTest, PreservesLegacyLiteralAndUnresolvedKeySpelling)
{
  auto read = access();
  read["name"] = "A";
  read["indices"] = Json::array({"0007", "i", "i*2"});
  EXPECT_EQ(
    unroll_node_actual(loop(2, Json::array({read})), Granularity::Element, 32),
    (std::vector<std::string>{"A-0007-0-i*2", "A-0007-1-i*2"}));
}

}  // namespace
