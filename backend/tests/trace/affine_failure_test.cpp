#include "../../src/trace/call_substitution.hpp"
#include "prepared_access_test_support.hpp"

#include <limits>

namespace yarda::test
{
namespace
{
using namespace prepared_access;

TEST(AffineFailureTest, SubstitutionOverflowCannotCaptureCallerFormalName)
{
  const auto max = std::to_string(std::numeric_limits<std::int64_t>::max());
  auto node = detail::substitute_call_node(access("global::A", "2*x+1"),
                                           {{"x", max}}, {}, {});
  const auto raw = kernel(Json::array({loop(1, Json::array({node}), "x")}));
  expect_resolution_failure(raw, addresses(),
                            "runtime-dependent index cannot be resolved", 0,
                            ResolutionCategory::Unsupported);
}

TEST(AffineFailureTest, FailedInlineCompositionDoesNotRejectZeroTripBody)
{
  auto helper = function("helper", Json::array({access("global::A", "2*x+1")}),
                         "ape.inline");
  helper["params"] = {"x"};
  auto invocation = call("helper");
  invocation["args"] = {"9223372036854775807"};
  auto raw =
    kernel(Json::array({loop(0, Json::array({invocation})), access()}));
  raw["functions"].push_back(helper);
  const auto result = resolved_task_traces(raw, addresses());
  expect_offsets(result.tasks[0], {0});
}

TEST(AffineFailureTest, NeverTreatsOneUnboundIndexAsAnExactRow)
{
  for (const bool missingFirst : {false, true})
  {
    auto node = access();
    node["indices"] =
      missingFirst ? Json{"2*i+missing", "2*i"} : Json{"2*i", "2*i+missing"};
    expect_resolution_failure(
      kernel(Json::array({loop(1, Json::array({node}))})), addresses(),
      "runtime-dependent index cannot be resolved", 0,
      ResolutionCategory::Unsupported);
  }
}

TEST(AffineFailureTest, PreservesOperationDiagnosticBeforeAffineFailure)
{
  const auto node = access("global::A", "2*i+missing", "atomic");
  expect_resolution_failure(kernel(Json::array({loop(1, Json::array({node}))})),
                            addresses(), "unsupported memory operation: atomic",
                            0, ResolutionCategory::Unsupported);
}

TEST(AffineFailureTest, SubstitutesMultipleFormalsSimultaneously)
{
  const auto result = detail::substitute_call_node(
    access("global::A", "2*x+y"), {{"x", "y+1"}, {"y", "i+1"}}, {}, {});
  EXPECT_EQ(result["indices"][0], "i+2*y+3");
}

TEST(AffineFailureTest, NormalizationFailureCannotMaskEarlierCallbackException)
{
  auto raw = kernel(Json::array({access(), access("global::A", "2*i+")}));
  auto sink = discard_sink();
  sink.access = [](const std::string &, const ResolvedAccess &) { throw 37; };
  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(), sink), int);
}

}  // namespace
}  // namespace yarda::test
