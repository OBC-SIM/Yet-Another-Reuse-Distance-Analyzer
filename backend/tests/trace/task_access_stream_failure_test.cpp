#include <stdexcept>

#include "task_access_stream_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::stream;

class StreamCallbackFailureTest : public ::testing::TestWithParam<int>
{
};

TEST_P(StreamCallbackFailureTest, PropagatesConsumerExceptionAndStopsCallbacks)
{
  const auto raw = module(Json::array({
    function("first", Json::array({access(), access()})),
    function("second", Json::array({access()})),
  }));
  Collector collector;
  auto sink = collector.sink();
  struct ConsumerError : std::runtime_error
  {
    ConsumerError() : std::runtime_error("consumer stopped") {}
  };
  if (GetParam() == 0)
    sink.begin_task = [](const std::string &, std::uint64_t) {
      throw ConsumerError{};
    };
  if (GetParam() == 1)
    sink.access = [](const std::string &, const ResolvedAccess &) {
      throw ConsumerError{};
    };
  if (GetParam() == 2)
    sink.end_task = [](const std::string &, const TraceCoverage &) {
      throw ConsumerError{};
    };

  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(), sink),
               ConsumerError);
  const std::vector<std::vector<std::string>> expected{
    {},
    {"begin:first"},
    {"begin:first", "access:first:0", "access:first:1"},
  };
  EXPECT_EQ(collector.notifications,
            expected.at(static_cast<std::size_t>(GetParam())));
}

TEST_P(StreamCallbackFailureTest,
       DoesNotRelabelConsumerJsonExceptionAsMalformedLat)
{
  const auto raw = module(Json::array({
    function("first", Json::array({access()})),
    function("second", Json::array({access()})),
  }));
  Collector collector;
  auto sink = collector.sink();
  const auto fail = [] { static_cast<void>(Json::object().at("sink-token")); };
  if (GetParam() == 0)
    sink.begin_task = [&](const std::string &, std::uint64_t) { fail(); };
  if (GetParam() == 1)
    sink.access = [&](const std::string &, const ResolvedAccess &) { fail(); };
  if (GetParam() == 2)
    sink.end_task = [&](const std::string &, const TraceCoverage &) { fail(); };

  try
  {
    static_cast<void>(stream_resolved_task_accesses(raw, addresses(), sink));
    FAIL() << "expected consumer JSON exception";
  }
  catch (const Json::out_of_range & error)
  {
    EXPECT_EQ(error.id, 403);
    EXPECT_NE(std::string(error.what()).find("sink-token"), std::string::npos);
  }
  const std::vector<std::vector<std::string>> expected{
    {},
    {"begin:first"},
    {"begin:first", "access:first:0"},
  };
  EXPECT_EQ(collector.notifications,
            expected.at(static_cast<std::size_t>(GetParam())));
}

TEST_P(StreamCallbackFailureTest, RejectsMissingCallbackBeforeTaskBegins)
{
  const auto raw = module(Json::array({function("empty", Json::array())}));
  Collector collector;
  auto sink = collector.sink();
  if (GetParam() == 0) sink.begin_task = {};
  if (GetParam() == 1) sink.access = {};
  if (GetParam() == 2) sink.end_task = {};

  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(), sink),
               std::invalid_argument);
  EXPECT_TRUE(collector.notifications.empty());
}

INSTANTIATE_TEST_SUITE_P(AllCallbacks, StreamCallbackFailureTest,
                         ::testing::Values(0, 1, 2));

TEST(TaskAccessStreamFailureTest,
     StopsOnLaterTaskConsumerFailureAfterEarlierTaskCompletes)
{
  const auto raw = module(Json::array({
    function("first", Json::array({access()})),
    function("second", Json::array({access()})),
    function("third", Json::array({access()})),
  }));
  Collector collector;
  auto sink = collector.sink();
  const auto record_access = sink.access;
  struct ConsumerError : std::runtime_error
  {
    ConsumerError() : std::runtime_error("second task failed") {}
  };
  sink.access = [&](const std::string & id, const ResolvedAccess & value) {
    if (id == "second") throw ConsumerError{};
    record_access(id, value);
  };

  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(), sink),
               ConsumerError);
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:first", "access:first:0",
                                      "end:first", "begin:second"}));
}

TEST(TaskAccessStreamFailureTest,
     PreservesBudgetedLineConsumerExceptionAndStopsInsideSpan)
{
  const auto raw = module(Json::array({
                            function("kernel", Json::array({access()})),
                            function("later", Json::array({access()})),
                          }),
                          40);
  TraceEmissionBudget budget({10, 10});
  Collector collector;
  auto sink = collector.sink();
  struct Stop
  {
  };
  std::uint64_t count = 0;
  sink.access = [&](const std::string &, const ResolvedAccess & value) {
    for_each_cache_line(
      value, {32, 8, 2},
      [&](const CacheLineMapping & line) {
        ++count;
        if (line.line_span_ordinal == 1) throw Stop{};
      },
      budget);
  };

  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(40), sink, budget),
               Stop);
  EXPECT_EQ(count, 2U);
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(TaskAccessStreamFailureTest, PreservesTaskLocalFailureAndModuleCoverage)
{
  const auto raw = module(Json::array({
    function("first", Json::array({access()})),
    function("second", Json::array({access("global::missing"), access()})),
    function("third", Json::array({access()})),
  }));
  Collector collector;
  try
  {
    static_cast<void>(
      stream_resolved_task_accesses(raw, addresses(), collector.sink()));
    FAIL() << "expected resolution failure";
  }
  catch (const ResolutionError & error)
  {
    EXPECT_EQ(error.task_id(), "second");
    EXPECT_EQ(error.source_access_ordinal(), 0U);
    EXPECT_EQ(error.object_id(), "global::missing");
    EXPECT_EQ(error.category(), ResolutionCategory::Unresolved);
    expect_coverage(error.coverage(), {2, 1, 1, 0});
  }
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:first", "access:first:0",
                                      "end:first", "begin:second"}));
}

TEST(TaskAccessStreamFailureTest, ConvertsMalformedLoopJsonToInputError)
{
  auto invalid_loop = loop(1, Json::array({access()}));
  invalid_loop["bound"] = "unknown";
  const auto raw = module(Json::array({
    function("kernel", Json::array({invalid_loop, access()})),
  }));
  Collector collector;
  try
  {
    static_cast<void>(
      stream_resolved_task_accesses(raw, addresses(), collector.sink()));
    FAIL() << "expected malformed input";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_NE(std::string(error.what()).find("malformed LAT input"),
              std::string::npos);
  }
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

class StreamInvalidModuleTest : public ::testing::TestWithParam<int>
{
};

TEST_P(StreamInvalidModuleTest, RejectsInvalidTaskSelectionBeforeCallbacks)
{
  auto raw = module(Json::array({
    function("kernel", Json::array({access()})),
  }));
  switch (GetParam())
  {
    case 0:
      raw["functions"][0]["annotations"] = Json::array();
      break;
    case 1:
      raw["functions"].push_back(raw["functions"][0]);
      break;
    case 2:
      raw["functions"][0]["function"] = "";
      break;
    case 3:
      raw["functions"][0]["annotations"].push_back("ape.inline");
      break;
    case 4:
      raw["functions"][0]["body"].push_back(call("missing"));
      break;
    case 5:
      raw["functions"][0]["body"].push_back(call("helper"));
      raw["functions"].push_back(
        function("helper", Json::array({call("helper")}), "ape.inline"));
      break;
  }
  Collector collector;

  EXPECT_THROW(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()),
    std::invalid_argument);
  EXPECT_TRUE(collector.notifications.empty());
}

INSTANTIATE_TEST_SUITE_P(TaskValidation, StreamInvalidModuleTest,
                         ::testing::Values(0, 1, 2, 3, 4, 5));

}  // namespace
