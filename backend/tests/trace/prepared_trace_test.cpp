#include <limits>

#include "prepared_trace_test_support.hpp"
#include "work_limits_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::prepared;

TEST(PreparedTraceTest, RejectsUnknownNodeTypeAtVisit)
{
  const auto node = loop(1, Json::array({Json{{"type", "Widget"}}}));
  try
  {
    indices(node);
    FAIL() << "expected unsupported node failure";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_STREQ(error.what(), "Unknown LAT node type: Widget");
  }
}

TEST(PreparedTraceTest, RejectsMissingNodeTypeAtVisit)
{
  const auto node = loop(1, Json::array({Json::object()}));
  try
  {
    indices(node);
    FAIL() << "expected missing node type failure";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_STREQ(error.what(), "Unknown LAT node type: ");
  }
}

TEST(PreparedTraceTest, EmitsArrayWithoutIndicesAsOneEmptyRow)
{
  const Json node{{"type", "Array"}, {"name", "A"}, {"object", "global::A"}};
  EXPECT_EQ(indices(node), (IndexRows{{}}));
}

TEST(PreparedTraceTest, AllowsLoopWithoutBodyAtExactWorkLimit)
{
  const Json node{{"type", "Loop"}, {"var", "i"}, {"bound", 3}};
  EXPECT_TRUE(indices(node, {3, 3}).empty());
}

TEST(PreparedTraceTest, ChargesLoopWithoutBodyAgainstCumulativeLimit)
{
  const Json node{{"type", "Loop"}, {"var", "i"}, {"bound", 3}};
  test::work::expect_limit_error(
    [&] {
      indices(node, {3, 2});
    },
    "cumulative loop iteration count exceeds 2");
}

TEST(PreparedTraceTest, EmitsScalarWithoutReadingArrayIndices)
{
  const Json scalar{{"type", "Scalar"}, {"name", "s"}, {"indices", 42}};
  EXPECT_EQ(indices(scalar), (IndexRows{{}}));
}

TEST(PreparedTraceTest, PreservesDefaultStartAndStep)
{
  const Json node{{"type", "Loop"},
                  {"var", "i"},
                  {"bound", 3},
                  {"body", Json::array({access("global::A", "i")})}};
  EXPECT_EQ(indices(node), (IndexRows{{"0"}, {"1"}, {"2"}}));
}

TEST(PreparedTraceTest, PreservesPositiveNonUnitStep)
{
  EXPECT_EQ(
    indices(loop(7, Json::array({access("global::A", "i")}), "i", 1, 2)),
    (IndexRows{{"1"}, {"3"}, {"5"}}));
}

TEST(PreparedTraceTest, PreservesCacheLineSizedStep)
{
  EXPECT_EQ(
    indices(loop(97, Json::array({access("global::A", "i")}), "i", 0, 32)),
    (IndexRows{{"0"}, {"32"}, {"64"}, {"96"}}));
}

TEST(PreparedTraceTest, PreservesNegativeNonUnitStep)
{
  EXPECT_EQ(
    indices(loop(0, Json::array({access("global::A", "i")}), "i", 5, -2)),
    (IndexRows{{"5"}, {"3"}, {"1"}}));
}

TEST(PreparedTraceTest, HandlesExtremeSpanInThreeIterations)
{
  const auto min = std::numeric_limits<std::int64_t>::min();
  const auto max = std::numeric_limits<std::int64_t>::max();
  EXPECT_EQ(
    indices(loop(max, Json::array({access("global::A", "i")}), "i", min, max)),
    (IndexRows{{"-9223372036854775808"}, {"-1"}, {"9223372036854775806"}}));
}

TEST(PreparedTraceTest, HandlesMinimumSignedStepInTwoIterations)
{
  const auto min = std::numeric_limits<std::int64_t>::min();
  const auto max = std::numeric_limits<std::int64_t>::max();
  EXPECT_EQ(
    indices(loop(min, Json::array({access("global::A", "i")}), "i", max, min)),
    (IndexRows{{"9223372036854775807"}, {"-1"}}));
}

TEST(PreparedTraceTest, RestoresShadowedValueAfterNestedAndSiblingLoops)
{
  const auto read = access("global::A", "i");
  const auto node = loop(3,
                         Json::array({
                           read,
                           loop(12, Json::array({read}), "i", 10),
                           read,
                           loop(21, Json::array({read}), "i", 20),
                           read,
                         }),
                         "i", 1);
  EXPECT_EQ(indices(node), (IndexRows{{"1"},
                                      {"10"},
                                      {"11"},
                                      {"1"},
                                      {"20"},
                                      {"1"},
                                      {"2"},
                                      {"10"},
                                      {"11"},
                                      {"2"},
                                      {"20"},
                                      {"2"}}));
}

TEST(PreparedTraceTest, DoesNotLeakSiblingLoopBindings)
{
  const auto node =
    loop(2, Json::array({
              loop(4, Json::array({access("global::A", "j")}), "j", 3),
              access("global::A", "j"),
              access("global::A", "i+2"),
            }));
  EXPECT_EQ(indices(node),
            (IndexRows{{"3"}, {"j"}, {"2"}, {"3"}, {"j"}, {"3"}}));
}

TEST(PreparedTraceTest, OuterSlotsSurviveDeepSlotStorageGrowth)
{
  const auto read = access("global::A", "root");
  auto node = read;
  for (int depth = 0; depth < 40; ++depth)
    node = loop(1, Json::array({node}), "v" + std::to_string(depth));
  node = loop(4, Json::array({node, read}), "root", 2);
  EXPECT_EQ(indices(node), (IndexRows{{"2"}, {"2"}, {"3"}, {"3"}}));
}

TEST(PreparedTraceTest, ReusesExpressionsWithCurrentValuesAndFixedSpelling)
{
  auto read = access();
  read["indices"] = Json::array({"0007", "i+2", "i-1", "i*2"});
  EXPECT_EQ(indices(loop(3, Json::array({read}), "i", 1)),
            (IndexRows{{"0007", "3", "0", "i*2"}, {"0007", "4", "1", "i*2"}}));
}

TEST(PreparedTraceTest, RechecksIndexOverflowOnLaterIteration)
{
  EXPECT_EQ(indices(loop(
              2, Json::array({access("global::A", "i+9223372036854775807")}))),
            (IndexRows{{"9223372036854775807"}, {"i+9223372036854775807"}}));
}

TEST(PreparedTraceTest, DoesNotPrepareUnvisitedBody)
{
  auto malformed = loop(1, Json::array());
  malformed["bound"] = "runtime";
  auto read = access();
  read["indices"] = Json::array({42});
  EXPECT_TRUE(indices(loop(0, Json::array({malformed, read})), {0, 0}).empty());
}

TEST(PreparedTraceTest, ReservesEveryDynamicEntryEvenForEmptyLoops)
{
  const auto node = loop(2, Json::array({loop(3, Json::array(), "j")}));
  EXPECT_TRUE(indices(node, {3, 8}).empty());
  test::work::expect_limit_error(
    [&] {
      indices(node, {3, 7});
    },
    "cumulative loop iteration count exceeds 7");
}

TEST(PreparedTraceTest, NewTraversalHasFreshSlotsAndSharedLoopBudget)
{
  detail::ExpansionBudget budget(detail::kExpansionLimits, {1, 1});
  IndexRows rows;
  const auto sink = [&](const Json &, const std::vector<std::string> & value) {
    rows.push_back(value);
  };
  detail::visit_prepared_trace(
    loop(4, Json::array({access("global::A", "i")}), "i", 3), budget, sink);
  detail::visit_prepared_trace(access("global::A", "i"), budget, sink);
  EXPECT_EQ(rows, (IndexRows{{"3"}, {"i"}}));
  test::work::expect_limit_error(
    [&] { detail::visit_prepared_trace(loop(1, Json::array()), budget, sink); },
    "cumulative loop iteration count exceeds 1");
}

}  // namespace
