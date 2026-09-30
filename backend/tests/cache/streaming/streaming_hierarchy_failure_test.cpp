#include <stdexcept>

#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;

class StreamingHierarchyInvalidTaskTest : public ::testing::TestWithParam<int>
{
};

TEST_P(StreamingHierarchyInvalidTaskTest, RejectsTaskSelectionBeforeEvents)
{
  auto raw = byte_trace({0});
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
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(raw, byte_addresses(),
                                           make_batch_hierarchy(),
                                           collector.options()),
               std::invalid_argument);
  EXPECT_TRUE(collector.events.empty());
}

INSTANTIATE_TEST_SUITE_P(TaskSelection, StreamingHierarchyInvalidTaskTest,
                         ::testing::Values(0, 1, 2, 3, 4, 5));

class StreamingHierarchyInvalidGeometryTest
  : public ::testing::TestWithParam<int>
{
};

TEST_P(StreamingHierarchyInvalidGeometryTest, RejectsGeometryBeforeEvents)
{
  auto hierarchy = make_batch_hierarchy();
  switch (GetParam())
  {
    case 0:
      hierarchy.l1.geometry.line_size = 0;
      break;
    case 1:
      hierarchy.llc.geometry.line_count = 0;
      break;
    case 2:
      hierarchy.llc.geometry.line_size = 64;
      break;
    case 3:
      hierarchy.llc.geometry.associativity = 3;
      break;
  }
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(byte_trace({0}), byte_addresses(),
                                           hierarchy, collector.options()),
               std::invalid_argument);
  EXPECT_TRUE(collector.events.empty());
}

INSTANTIATE_TEST_SUITE_P(Geometry, StreamingHierarchyInvalidGeometryTest,
                         ::testing::Values(0, 1, 2, 3));

TEST(StreamingHierarchyFailureTest, RejectsRelativeAddresses)
{
  auto objects = byte_addresses();
  objects.basis = AddressBasis::ImageRelative;
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(byte_trace({0}), objects,
                                           make_batch_hierarchy(),
                                           collector.options()),
               std::invalid_argument);
  EXPECT_TRUE(collector.events.empty());
}

TEST(StreamingHierarchyFailureTest, RejectsOpaqueCallsInLaterTaskBeforeAccess)
{
  const auto raw = byte_module(Json::array({
    function("first", byte_body({0})),
    function("second", Json::array({access(), call("opaque")})),
    function("third", byte_body({0})),
    function("opaque", Json::array(), "unannotated"),
  }));
  EventCollector collector;
  std::optional<StreamingHierarchyResult> result;
  EXPECT_THROW(result = analyze_streaming_hierarchy(raw, byte_addresses(),
                                                    make_batch_hierarchy(),
                                                    collector.options()),
               std::invalid_argument);
  EXPECT_FALSE(result);
  ASSERT_EQ(collector.events.size(), 1U);
  EXPECT_EQ(collector.events[0].first, "first");
}

TEST(StreamingHierarchyFailureTest,
     LaterResolutionFailureDoesNotReturnPartialModule)
{
  const auto raw = byte_module(Json::array({
    function("first", byte_body({0})),
    function("second", Json::array({access("global::missing")})),
    function("third", byte_body({0})),
  }));
  EventCollector collector;
  std::optional<StreamingHierarchyResult> result;
  try
  {
    result = analyze_streaming_hierarchy(
      raw, byte_addresses(), make_batch_hierarchy(), collector.options());
    FAIL() << "expected source resolution failure";
  }
  catch (const ResolutionError & error)
  {
    EXPECT_EQ(error.task_id(), "second");
    EXPECT_EQ(error.source_access_ordinal(), 0U);
    EXPECT_EQ(error.object_id(), "global::missing");
    stream::expect_coverage(error.coverage(), (TraceCoverage{2, 1, 1, 0}));
  }
  EXPECT_FALSE(result);
  ASSERT_EQ(collector.events.size(), 1U);
  EXPECT_EQ(collector.events[0].first, "first");
}

TEST(StreamingHierarchyFailureTest,
     LaterSinkFailurePropagatesAndStopsAllDelivery)
{
  const auto raw = byte_module(Json::array({
    function("first", byte_body({0})),
    function("second", byte_body({0, 0})),
    function("third", byte_body({0})),
  }));
  struct SinkFailure : std::runtime_error
  {
    SinkFailure() : std::runtime_error("second-task-sink") {}
  };
  std::vector<std::string> ids;
  StreamingHierarchyOptions options;
  options.event_limit = 10;
  options.event_sink = [&](const std::string & id,
                           const HierarchyAccessEvent &) {
    ids.push_back(id);
    if (id == "second") throw SinkFailure{};
  };
  std::optional<StreamingHierarchyResult> result;
  EXPECT_THROW(result = analyze_streaming_hierarchy(
                 raw, byte_addresses(), make_batch_hierarchy(), options),
               SinkFailure);
  EXPECT_FALSE(result);
  EXPECT_EQ(ids, (std::vector<std::string>{"first", "second"}));
}

TEST(StreamingHierarchyFailureTest, DoesNotRelabelSinkJsonException)
{
  StreamingHierarchyOptions options;
  options.event_limit = 10;
  unsigned callbacks = 0;
  options.event_sink = [&](const std::string &, const HierarchyAccessEvent &) {
    ++callbacks;
    static_cast<void>(Json::object().at("event-sink-token"));
  };
  try
  {
    static_cast<void>(analyze_streaming_hierarchy(
      byte_trace({0, 0}), byte_addresses(), make_batch_hierarchy(), options));
    FAIL() << "expected sink JSON exception";
  }
  catch (const Json::out_of_range & error)
  {
    EXPECT_EQ(error.id, 403);
    EXPECT_NE(std::string(error.what()).find("event-sink-token"),
              std::string::npos);
  }
  EXPECT_EQ(callbacks, 1U);
}

TEST(StreamingHierarchyFailureTest, PreservesNonstandardFailureInsideLineSpan)
{
  const auto raw = stream::module(
    Json::array({function("wide", Json::array({access(), access()}))}), 40);
  struct Stop
  {
    int token;
  };
  StreamingHierarchyOptions options;
  options.event_limit = 10;
  unsigned callbacks = 0;
  options.event_sink = [&](const std::string &,
                           const HierarchyAccessEvent & event) {
    ++callbacks;
    if (event.l1_mapping.line_span_ordinal == 1) throw Stop{73};
  };
  try
  {
    static_cast<void>(analyze_streaming_hierarchy(
      raw, stream::addresses(40), make_batch_hierarchy(), options));
    FAIL() << "expected nonstandard sink exception";
  }
  catch (const Stop & error)
  {
    EXPECT_EQ(error.token, 73);
  }
  EXPECT_EQ(callbacks, 2U);
}

TEST(StreamingHierarchyFailureTest,
     RejectsMalformedLaterLoopWithoutPartialSuccess)
{
  auto invalid = loop(2, byte_body({0}));
  invalid["bound"] = "unknown";
  const auto raw = byte_module(
    Json::array({function("kernel", Json::array({access(), invalid}))}));
  EventCollector collector;
  EXPECT_THROW(analyze_streaming_hierarchy(raw, byte_addresses(),
                                           make_batch_hierarchy(),
                                           collector.options()),
               std::invalid_argument);
  EXPECT_EQ(collector.events.size(), 1U);
}

}  // namespace
