#include <limits>

#include "prepared_access_test_support.hpp"
#include "work_limits_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::prepared_access;
constexpr auto kMax = std::numeric_limits<std::uint64_t>::max();

TEST(PreparedAccessFailureTest, OperationFailurePrecedesRuntimeIndex)
{
  auto node = access("global::A", "i*2");
  node.erase("op");
  expect_resolution_failure(kernel({loop(1, {node})}), addresses(),
                            "load/store operation metadata is unavailable");
}

TEST(PreparedAccessFailureTest, RuntimeIndexPrecedesMissingObjectMetadata)
{
  auto raw = kernel({loop(1, {access("global::A", "i*2")})});
  raw["metadata"]["objects"].erase("global::A");
  expect_resolution_failure(raw, addresses(),
                            "runtime-dependent index cannot be resolved", 0,
                            ResolutionCategory::Unsupported);
}

TEST(PreparedAccessFailureTest, RejectsRowWithEarlierRuntimeDependentIndex)
{
  auto node = access("global::A", "n*2");
  node["indices"] = Json::array({"n*2", "i"});
  node["shape"] = Json::array({8, 8});
  expect_resolution_failure(kernel({loop(2, {node})}), addresses(),
                            "runtime-dependent index cannot be resolved", 0,
                            ResolutionCategory::Unsupported);
}

TEST(PreparedAccessFailureTest, RejectsRowWithLaterRuntimeDependentIndex)
{
  auto node = access("global::A", "i");
  node["indices"] = Json::array({"i", "n*2"});
  node["shape"] = Json::array({8, 8});
  expect_resolution_failure(kernel({loop(2, {node})}), addresses(),
                            "runtime-dependent index cannot be resolved", 0,
                            ResolutionCategory::Unsupported);
}

TEST(PreparedAccessFailureTest, InvalidByteRangePrecedesMissingElfSymbol)
{
  auto objects = addresses();
  objects.objects.erase("global::A");
  expect_resolution_failure(kernel({access("global::A", "-1")}), objects,
                            "access byte range is invalid");
}

TEST(PreparedAccessFailureTest, DynamicDimensionPrecedesMalformedLaterField)
{
  auto node = structured_access();
  node["access_path"][2]["name"] = "wrong";
  const auto raw =
    kernel({loop(3, {loop(1, {node}, "j")}, "i", 2)}, structured_module());
  expect_resolution_failure(raw, addresses(),
                            "structured access index exceeds its dimension: "
                            "global::A");
}

TEST(PreparedAccessFailureTest, SourceBudgetPrecedesMalformedLayoutPreparation)
{
  auto node = structured_access();
  node["access_path"][2]["name"] = "wrong";
  const auto raw =
    kernel({loop(1, {loop(1, {node}, "j")})}, structured_module());
  TraceEmissionBudget budget({0, 0});
  test::work::expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget);
    },
    "emitted source accesses exceeds 0");
}

TEST(PreparedAccessFailureTest, ZeroTripDoesNotPrepareInvalidStructuredLayout)
{
  auto node = structured_access();
  node["access_path"][2]["name"] = "wrong";
  const auto raw = kernel({loop(0, {node})}, structured_module());
  const auto result =
    stream_resolved_task_accesses(raw, addresses(), discard_sink());
  expect_coverage(result.coverage, {});
}

TEST(PreparedAccessFailureTest, CachedStructuredPlanRechecksNegativeIndex)
{
  const auto raw =
    kernel({loop(-2, {loop(1, {structured_access()}, "j")}, "i", 0, -1)},
           structured_module());
  expect_resolution_failure(raw, addresses(),
                            "structured access index is not an exact "
                            "non-negative integer: global::A",
                            1);
}

TEST(PreparedAccessFailureTest, CachedStructuredPlanRechecksDimension)
{
  const auto raw = kernel({loop(3, {loop(1, {structured_access()}, "j")})},
                          structured_module());
  expect_resolution_failure(raw, addresses(),
                            "structured access index exceeds its dimension: "
                            "global::A",
                            2);
}

TEST(PreparedAccessFailureTest, CachedLegacyPlanRechecksNegativeByteRange)
{
  expect_resolution_failure(
    kernel({loop(-2, {access("global::A", "i")}, "i", 0, -1)}), addresses(),
    "access byte range is invalid", 1);
}

TEST(PreparedAccessFailureTest, CachedElfBindingRechecksAddressAdditionOverflow)
{
  const auto raw =
    kernel({loop(5, {access("global::A", "i")})}, module(Json::array(), 1));
  auto objects = addresses(1);
  objects.objects["global::A"] = {kMax - 3, 64};
  expect_resolution_failure(raw, objects, "ELF object address overflows", 4);
}

TEST(PreparedAccessFailureTest, CachedElfBindingRechecksFinalByteRangeOverflow)
{
  const auto raw =
    kernel({loop(3, {access("global::A", "i")})}, module(Json::array(), 8));
  auto objects = addresses(8);
  objects.objects["global::A"] = {kMax - 11, 64};
  expect_resolution_failure(raw, objects, "linked access range overflows", 1);
}

TEST(PreparedAccessFailureTest, CachedPlanKeepsOverflowingIndexCategory)
{
  const auto raw =
    kernel({loop(2, {access("global::A", "i+9223372036854775807")})},
           module(Json::array(), 1));
  auto objects = addresses(1);
  objects.objects["global::A"] = {0, kMax};
  expect_resolution_failure(raw, objects,
                            "runtime-dependent index cannot be resolved", 1,
                            ResolutionCategory::Unsupported);
}

TEST(PreparedAccessFailureTest, ConsumerExceptionPrecedesMalformedLaterLayout)
{
  auto malformed = structured_access();
  malformed["access_path"][2]["name"] = "wrong";
  const auto raw =
    kernel({loop(1, {loop(1, {structured_access(), malformed}, "j")})},
           structured_module());
  Collector collector;
  auto sink = collector.sink();
  const auto record = sink.access;
  sink.access = [&](const std::string & id, const ResolvedAccess & value) {
    record(id, value);
    throw 42;
  };
  try
  {
    stream_resolved_task_accesses(raw, addresses(), sink);
    FAIL() << "expected consumer exception";
  }
  catch (int token)
  {
    EXPECT_EQ(token, 42);
  }
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "access:kernel:0"}));
}

}  // namespace
