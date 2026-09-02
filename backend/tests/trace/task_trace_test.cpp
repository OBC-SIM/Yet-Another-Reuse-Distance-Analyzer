#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "yarda/cache/lru_rd_analysis.hpp"
#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/resolution_error.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/resolved_mapping.hpp"

namespace
{

using Json = nlohmann::json;

template <typename Trace, typename = void>
struct CanFlattenMappedTraces : std::false_type
{
};

template <typename Trace>
struct CanFlattenMappedTraces<
  Trace,
  std::void_t<decltype(yarda::flatten_mapped_traces(std::declval<Trace>()))>>
  : std::true_type
{
};

static_assert(
  !CanFlattenMappedTraces<const std::vector<yarda::MappedTaskTrace> &>::value);
static_assert(
  CanFlattenMappedTraces<const std::vector<yarda::NamedMappedTrace> &>::value);

Json access(const std::string & object, const std::string & index = "0")
{
  return {
    {"type", "Array"},  {"name", object},
    {"object", object}, {"indices", Json::array({index})},
    {"op", "load"},
  };
}

Json metadata(const std::initializer_list<std::string> & objects)
{
  Json result = Json::object();
  for (const auto & object : objects)
  {
    result[object] = {
      {"kind", "array"}, {"shape", Json::array({4})}, {"elem_size", 4}};
  }
  return result;
}

Json function(std::string name, Json body, std::string role)
{
  return {
    {"function", std::move(name)},
    {"annotations", Json::array({std::move(role)})},
    {"body", std::move(body)},
  };
}

Json module(Json functions, Json objects)
{
  return {
    {"schema_version", 2},
    {"metadata", {{"objects", std::move(objects)}}},
    {"functions", std::move(functions)},
  };
}

yarda::ObjectAddressModel addresses(
  const std::initializer_list<std::pair<std::string, std::uint64_t>> & objects)
{
  yarda::ObjectAddressModel result;
  for (const auto & [object, base] : objects)
  {
    result.objects[object] = {base, 16};
  }
  return result;
}

TEST(TaskTraceTest, KeepsAnalyzedRootsSeparateAndResetsOrdinals)
{
  const auto raw = module(
    Json::array(
      {function("first",
                Json::array({access("global::A"), access("global::A")}),
                "ape.analyze"),
       function("second", Json::array({access("global::A")}), "ape.analyze")}),
    metadata({"global::A"}));

  const auto result =
    yarda::resolved_task_traces(raw, addresses({{"global::A", 0x1000}}));

  ASSERT_EQ(result.tasks.size(), 2U);
  EXPECT_EQ(result.tasks[0].task_id, "first");
  EXPECT_EQ(result.tasks[1].task_id, "second");
  ASSERT_EQ(result.tasks[0].accesses.size(), 2U);
  ASSERT_EQ(result.tasks[1].accesses.size(), 1U);
  EXPECT_EQ(result.tasks[0].accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.tasks[0].accesses[1].source_access_ordinal, 1U);
  EXPECT_EQ(result.tasks[1].accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.tasks[0].coverage.source_accesses, 2U);
  EXPECT_EQ(result.tasks[1].coverage.source_accesses, 1U);
  EXPECT_EQ(result.coverage.source_accesses, 3U);
  EXPECT_TRUE(result.coverage.complete());
}

TEST(TaskTraceTest, PreservesNestedCallSiteAndLoopOrderWithinOneTask)
{
  const Json call_inner = {
    {"type", "Call"},
    {"callee", "inner"},
    {"args", Json::array()},
  };
  const Json call_outer = {
    {"type", "Call"},
    {"callee", "outer"},
    {"args", Json::array()},
  };
  const Json loop = {
    {"type", "Loop"},
    {"var", "i"},
    {"bound", 2},
    {"body", Json::array({access("global::B", "i")})},
  };
  const auto raw = module(
    Json::array(
      {function("inner", Json::array({access("global::C")}), "ape.inline"),
       function(
         "outer",
         Json::array({access("global::B"), call_inner, access("global::D")}),
         "ape.inline"),
       function("kernel",
                Json::array({access("global::A"), call_outer, loop,
                             access("global::A", "1")}),
                "ape.analyze")}),
    metadata({"global::A", "global::B", "global::C", "global::D"}));

  const auto result =
    yarda::resolved_task_traces(raw, addresses({{"global::A", 0x1000},
                                                {"global::B", 0x1100},
                                                {"global::C", 0x1200},
                                                {"global::D", 0x1300}}));

  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].task_id, "kernel");
  ASSERT_EQ(result.tasks[0].accesses.size(), 7U);
  EXPECT_EQ(result.tasks[0].accesses[0].object_id, "global::A");
  EXPECT_EQ(result.tasks[0].accesses[1].object_id, "global::B");
  EXPECT_EQ(result.tasks[0].accesses[2].object_id, "global::C");
  EXPECT_EQ(result.tasks[0].accesses[3].object_id, "global::D");
  EXPECT_EQ(result.tasks[0].accesses[4].object_id, "global::B");
  EXPECT_EQ(result.tasks[0].accesses[5].object_id, "global::B");
  EXPECT_EQ(result.tasks[0].accesses[6].object_id, "global::A");
  EXPECT_EQ(result.tasks[0].accesses[1].object_byte_offset, 0U);
  EXPECT_EQ(result.tasks[0].accesses[4].object_byte_offset, 0U);
  EXPECT_EQ(result.tasks[0].accesses[5].object_byte_offset, 4U);
}

TEST(TaskTraceTest, MapsAndAnalyzesEachTaskWithColdState)
{
  const auto raw = module(
    Json::array(
      {function("first",
                Json::array({access("global::A"), access("global::A")}),
                "yard.analyze"),
       function("second", Json::array({access("global::A")}), "yard.analyze")}),
    metadata({"global::A"}));
  const yarda::CacheGeometry geometry{64, 1, 1};
  const auto resolved =
    yarda::resolved_task_traces(raw, addresses({{"global::A", 0x1000}}));

  const auto mapped = yarda::map_resolved_task_traces(resolved, geometry);
  const auto directly_mapped = yarda::mapped_task_traces(
    raw, geometry, addresses({{"global::A", 0x1000}}));

  ASSERT_EQ(mapped.tasks.size(), 2U);
  ASSERT_EQ(directly_mapped.tasks.size(), 2U);
  EXPECT_EQ(mapped.tasks[0].coverage.emitted_line_references, 2U);
  EXPECT_EQ(mapped.tasks[1].coverage.emitted_line_references, 1U);
  EXPECT_EQ(mapped.coverage.emitted_line_references, 3U);
  ASSERT_EQ(mapped.mappings.size(), 1U);
  ASSERT_EQ(directly_mapped.mappings.size(), 1U);
  EXPECT_EQ(mapped.mappings.begin()->first,
            directly_mapped.mappings.begin()->first);
  EXPECT_EQ(mapped.mappings.begin()->second.decoded.address,
            directly_mapped.mappings.begin()->second.decoded.address);
  for (std::size_t index = 0; index < mapped.tasks.size(); ++index)
  {
    const auto & remapped = mapped.tasks[index];
    const auto & direct = directly_mapped.tasks[index];
    EXPECT_EQ(remapped.task_id, direct.task_id);
    EXPECT_EQ(remapped.accesses.size(), direct.accesses.size());
    EXPECT_EQ(remapped.coverage.source_accesses,
              direct.coverage.source_accesses);
    EXPECT_EQ(remapped.coverage.resolved_accesses,
              direct.coverage.resolved_accesses);
    EXPECT_EQ(remapped.coverage.rejected_accesses,
              direct.coverage.rejected_accesses);
    EXPECT_EQ(remapped.coverage.emitted_line_references,
              direct.coverage.emitted_line_references);
  }
  ASSERT_EQ(directly_mapped.tasks[1].accesses.size(), 1U);
  EXPECT_EQ(directly_mapped.tasks[1].accesses[0].source_access_ordinal, 0U);
  const auto first =
    yarda::analyze_lru_reuse(mapped.tasks[0].accesses, geometry);
  const auto second =
    yarda::analyze_lru_reuse(mapped.tasks[1].accesses, geometry);
  ASSERT_EQ(first.accesses.size(), 2U);
  ASSERT_EQ(second.accesses.size(), 1U);
  EXPECT_EQ(first.accesses[0].outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(first.accesses[1].outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(second.accesses[0].outcome, yarda::LruAccessOutcome::ColdMiss);
}

TEST(TaskTraceTest, ReportsTaskLocalOrdinalWithAggregateFailureCoverage)
{
  const auto raw = module(
    Json::array(
      {function("first", Json::array({access("global::A")}), "ape.analyze"),
       function("second", Json::array({access("global::missing")}),
                "ape.analyze")}),
    metadata({"global::A", "global::missing"}));

  try
  {
    static_cast<void>(
      yarda::resolved_task_traces(raw, addresses({{"global::A", 0x1000}})));
    FAIL() << "expected missing object rejection";
  }
  catch (const yarda::ResolutionError & error)
  {
    EXPECT_EQ(error.task_id(), "second");
    EXPECT_EQ(error.source_access_ordinal(), 0U);
    EXPECT_EQ(error.coverage().source_accesses, 2U);
    EXPECT_EQ(error.coverage().resolved_accesses, 1U);
    EXPECT_EQ(error.coverage().rejected_accesses, 1U);
  }
}

TEST(TaskTraceTest, RetainsEmptyAnalyzedTask)
{
  const auto raw =
    module(Json::array({function("empty", Json::array(), "ape.analyze")}),
           Json::object());

  const auto result =
    yarda::resolved_task_traces(raw, yarda::ObjectAddressModel{});

  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].task_id, "empty");
  EXPECT_TRUE(result.tasks[0].accesses.empty());
  EXPECT_TRUE(result.tasks[0].coverage.complete());
}

TEST(TaskTraceTest, RejectsDuplicateTaskIdentity)
{
  const auto raw =
    module(Json::array({function("duplicate", Json::array(), "ape.analyze"),
                        function("duplicate", Json::array(), "ape.analyze")}),
           Json::object());

  EXPECT_THROW(yarda::resolved_task_traces(raw, yarda::ObjectAddressModel{}),
               std::invalid_argument);
}

TEST(TaskTraceTest, RejectsEmptyTaskIdentity)
{
  const auto raw = module(
    Json::array({function("", Json::array(), "ape.analyze")}), Json::object());

  EXPECT_THROW(yarda::resolved_task_traces(raw, yarda::ObjectAddressModel{}),
               std::invalid_argument);
}

}  // namespace
