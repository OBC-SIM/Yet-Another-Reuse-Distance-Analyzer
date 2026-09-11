#include "prepared_trace_test_support.hpp"
#include "work_limits_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::prepared;
using yarda::test::work::expect_limit_error;

TEST(PreparedTraceFailureTest, ZeroTripBodyDoesNotReportExecutionErrors)
{
  auto bad_loop = loop(1, Json::array());
  bad_loop["bound"] = "runtime";
  auto bad_access = access();
  bad_access["indices"] = Json::array({42});
  const auto raw = module(Json::array({function(
    "kernel",
    Json::array({
      loop(0, Json::array({bad_loop, bad_access, {{"type", "Unknown"}}})),
    }))}));
  Collector collector;
  TraceEmissionBudget budget({0, 0});
  const auto result = stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {0, 0});
  expect_coverage(result.coverage, {});
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "end:kernel"}));
}

TEST(PreparedTraceFailureTest, ConsumerJsonExceptionPrecedesMalformedSibling)
{
  auto bad_access = access();
  bad_access["indices"] = Json::array({42});
  const auto raw = module(Json::array(
    {function("kernel", Json::array({
                          loop(2, Json::array({access(), bad_access})),
                        }))}));
  Collector collector;
  auto sink = collector.sink();
  const auto record = sink.access;
  sink.access = [&](const std::string & id, const ResolvedAccess & value) {
    record(id, value);
    static_cast<void>(Json::object().at("consumer-token"));
  };
  try
  {
    stream_resolved_task_accesses(raw, addresses(), sink);
    FAIL() << "expected consumer failure";
  }
  catch (const Json::out_of_range & error)
  {
    EXPECT_EQ(error.id, 403);
    EXPECT_NE(std::string(error.what()).find("consumer-token"),
              std::string::npos);
  }
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "access:kernel:0"}));
}

TEST(PreparedTraceFailureTest, LaterMalformedLoopWaitsForEarlierAccess)
{
  auto bad_loop = loop(1, Json::array());
  bad_loop["bound"] = "runtime";
  const auto raw = module(Json::array(
    {function("kernel", Json::array({
                          loop(2, Json::array({access(), bad_loop})),
                        }))}));
  Collector collector;
  try
  {
    stream_resolved_task_accesses(raw, addresses(), collector.sink());
    FAIL() << "expected malformed loop";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_EQ(std::string(error.what()).find("malformed LAT input: "), 0U);
  }
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "access:kernel:0"}));
}

TEST(PreparedTraceFailureTest, MalformedIndexTypePrecedesSourceCharging)
{
  auto read = access();
  read["indices"] = Json::array({42});
  const auto raw =
    module(Json::array({function("kernel", Json::array({
                                             loop(1, Json::array({read})),
                                           }))}));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  try
  {
    stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget);
    FAIL() << "expected malformed indices";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_EQ(std::string(error.what()).find("malformed LAT input: "), 0U);
  }
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(PreparedTraceFailureTest, SourceChargePrecedesUnsupportedExpression)
{
  const auto raw = module(Json::array(
    {function("kernel", Json::array({
                          loop(1, Json::array({access("global::A", "2*i")})),
                        }))}));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget);
    },
    "emitted source accesses exceeds 0");
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(PreparedTraceFailureTest, SourceChargePrecedesOverflowResolution)
{
  const auto raw = module(Json::array({function(
    "kernel",
    Json::array({
      loop(0, Json::array({access("global::A", "i-9223372036854775808")}), "i",
           -1),
    }))}));
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget);
    },
    "emitted source accesses exceeds 0");
}

TEST(PreparedTraceFailureTest, OverflowRetainsResolutionCategoryAndCoverage)
{
  const auto raw = module(Json::array({function(
    "kernel",
    Json::array({
      loop(0, Json::array({access("global::A", "i-9223372036854775808")}), "i",
           -1),
    }))}));
  Collector collector;
  try
  {
    stream_resolved_task_accesses(raw, addresses(), collector.sink());
    FAIL() << "expected unsupported overflowing index";
  }
  catch (const ResolutionError & error)
  {
    EXPECT_EQ(error.category(), ResolutionCategory::Unsupported);
    EXPECT_EQ(error.task_id(), "kernel");
    EXPECT_EQ(error.source_access_ordinal(), 0U);
    EXPECT_EQ(error.object_id(), "global::A");
    expect_coverage(error.coverage(), {1, 0, 1, 0});
    EXPECT_EQ(std::string(error.what()),
              "unsupported task access [task=kernel, ordinal=0, "
              "object=global::A]: runtime-dependent index cannot be resolved");
  }
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(PreparedTraceFailureTest, OuterReservationPrecedesMalformedInnerLoop)
{
  auto bad_loop = loop(1, Json::array());
  bad_loop["bound"] = "runtime";
  const auto raw =
    module(Json::array({function("kernel", Json::array({
                                             loop(2, Json::array({bad_loop})),
                                           }))}));
  Collector collector;
  TraceEmissionBudget budget;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget,
                                    {1, 0});
    },
    "loop iteration count exceeds 1");
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(PreparedTraceFailureTest, UnknownCallInZeroTripBodyStillPrecedesBegin)
{
  const auto raw =
    module(Json::array({function("kernel", Json::array({
                                             loop(0, Json::array({call("missin"
                                                                       "g")})),
                                           }))}));
  Collector collector;
  expect_limit_error(
    [&] { stream_resolved_task_accesses(raw, addresses(), collector.sink()); },
    "Unknown call target: missing");
  EXPECT_TRUE(collector.notifications.empty());
}

TEST(PreparedTraceFailureTest, LaterTaskResolutionKeepsModuleCoverage)
{
  const auto raw = module(Json::array({
    function("first", Json::array({loop(2, Json::array({access()}))})),
    function("second", Json::array({loop(2, Json::array({access("global::"
                                                                "missin"
                                                                "g")}))})),
    function("third", Json::array({access()})),
  }));
  Collector collector;
  try
  {
    stream_resolved_task_accesses(raw, addresses(), collector.sink());
    FAIL() << "expected missing object";
  }
  catch (const ResolutionError & error)
  {
    EXPECT_EQ(error.category(), ResolutionCategory::Unresolved);
    EXPECT_EQ(error.task_id(), "second");
    EXPECT_EQ(error.source_access_ordinal(), 0U);
    EXPECT_EQ(error.object_id(), "global::missing");
    expect_coverage(error.coverage(), {3, 2, 1, 0});
  }
  EXPECT_EQ(
    collector.notifications,
    (std::vector<std::string>{"begin:first", "access:first:0", "access:first:1",
                              "end:first", "begin:second"}));
}

}  // namespace
