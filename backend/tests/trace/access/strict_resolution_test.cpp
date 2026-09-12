#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <optional>
#include <utility>

#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/resolution_error.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace
{

using Json = nlohmann::json;

Json task_module(Json body, Json objects = Json::object(),
                 Json structures = Json::object())
{
  return {
    {"metadata",
     {{"objects", std::move(objects)}, {"structs", std::move(structures)}}},
    {"functions", Json::array({{{"function", "kernel"},
                                {"annotations", Json::array({"ape.analyze"})},
                                {"body", std::move(body)}}})},
  };
}

Json array_access(std::string object, std::string index,
                  std::string operation = "load")
{
  return {
    {"type", "Array"},
    {"name", object},
    {"object", std::move(object)},
    {"indices", Json::array({std::move(index)})},
    {"shape", Json::array({4})},
    {"elem_size", 4},
    {"op", std::move(operation)},
  };
}

template <typename Action>
std::optional<yarda::ResolutionError> capture_resolution_error(Action action)
{
  try
  {
    action();
  }
  catch (const yarda::ResolutionError & error)
  {
    return error;
  }
  catch (const std::exception & error)
  {
    ADD_FAILURE() << "unexpected exception type: " << error.what();
    return std::nullopt;
  }
  ADD_FAILURE() << "expected ResolutionError";
  return std::nullopt;
}

TEST(StrictResolutionTest, RejectsNonGlobalScalarAsUnsupported)
{
  const Json access = {
    {"type", "Scalar"},
    {"name", "local"},
    {"object", "function:kernel::local:local"},
    {"elem_size", 4},
    {"op", "load"},
  };

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      task_module(Json::array({access})), yarda::ObjectAddressModel{}));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unsupported);
  EXPECT_EQ(error->task_id(), "kernel");
  EXPECT_EQ(error->source_access_ordinal(), 0U);
  EXPECT_EQ(error->object_id(), "function:kernel::local:local");
  EXPECT_EQ(error->coverage().source_accesses, 1U);
  EXPECT_EQ(error->coverage().resolved_accesses, 0U);
  EXPECT_EQ(error->coverage().rejected_accesses, 1U);
  EXPECT_FALSE(error->coverage().complete());
}

TEST(StrictResolutionTest, RejectsRuntimeDependentIndexAsUnsupported)
{
  const auto first = array_access("global::A", "0");
  const auto dynamic = array_access("global::A", "runtime_index");
  const Json metadata = {
    {"global::A",
     {{"kind", "array"}, {"shape", Json::array({4})}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 16};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      task_module(Json::array({first, dynamic}), metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unsupported);
  EXPECT_EQ(error->task_id(), "kernel");
  EXPECT_EQ(error->source_access_ordinal(), 1U);
  EXPECT_EQ(error->object_id(), "global::A");
  EXPECT_EQ(error->coverage().source_accesses, 2U);
  EXPECT_EQ(error->coverage().resolved_accesses, 1U);
  EXPECT_EQ(error->coverage().rejected_accesses, 1U);
  EXPECT_FALSE(error->coverage().complete());
}

TEST(StrictResolutionTest, RejectsPointerBackedObjectAsUnsupported)
{
  const auto access = array_access("global::pointer", "0");
  const Json metadata = {
    {"global::pointer",
     {{"kind", "pointer"}, {"shape", Json::array({4})}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::pointer"] = {0x1000, 16};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      task_module(Json::array({access}), metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unsupported);
  EXPECT_EQ(error->object_id(), "global::pointer");
}

TEST(StrictResolutionTest, RejectsMissingOperationAsUnresolved)
{
  auto access = array_access("global::A", "0");
  access.erase("op");
  const Json metadata = {
    {"global::A",
     {{"kind", "array"}, {"shape", Json::array({4})}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 16};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      task_module(Json::array({access}), metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unresolved);
  EXPECT_EQ(error->source_access_ordinal(), 0U);
}

TEST(StrictResolutionTest, RejectsMalformedStructuredLayoutAsUnresolved)
{
  const Json access = {
    {"type", "Array"},
    {"name", "record.value"},
    {"object", "global::record"},
    {"indices", Json::array()},
    {"op", "load"},
    {"access_path",
     Json::array({{{"kind", "field"}, {"name", "value"}, {"index", 0}}})},
  };
  const Json metadata = {
    {"global::record",
     {{"kind", "struct"}, {"elem_type", "Missing"}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::record"] = {0x1000, 4};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      task_module(Json::array({access}), metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unresolved);
  EXPECT_EQ(error->task_id(), "kernel");
  EXPECT_EQ(error->object_id(), "global::record");
}

TEST(StrictResolutionTest, CountsOnlySelectedAnalyzedRoots)
{
  const Json metadata = {
    {"global::A",
     {{"kind", "array"}, {"shape", Json::array({4})}, {"elem_size", 4}}},
    {"global::B",
     {{"kind", "array"}, {"shape", Json::array({4})}, {"elem_size", 4}}},
  };
  const Json raw = {
    {"metadata", {{"objects", metadata}}},
    {"functions",
     Json::array({{{"function", "kernel"},
                   {"annotations", Json::array({"ape.analyze"})},
                   {"body", Json::array({array_access("global::A", "0")})}},
                  {{"function", "helper"},
                   {"body", Json::array({array_access("global::B", "0")})}}})},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 16};
  addresses.objects["global::B"] = {0x2000, 16};

  const auto result = yarda::resolved_block_traces(raw, addresses);

  ASSERT_EQ(result.traces.size(), 1U);
  ASSERT_EQ(result.traces[0].accesses.size(), 1U);
  EXPECT_EQ(result.traces[0].accesses[0].object_id, "global::A");
  EXPECT_EQ(result.coverage.source_accesses, 1U);
  EXPECT_EQ(result.coverage.resolved_accesses, 1U);
  EXPECT_TRUE(result.coverage.complete());
}

TEST(StrictResolutionTest, ReportsCompleteResolvedAndMappedCoverage)
{
  const Json scalar = {
    {"type", "Scalar"}, {"name", "flag"}, {"object", "global::flag"},
    {"elem_size", 4},   {"op", "load"},
  };
  const Json spanning = {
    {"type", "Array"},
    {"name", "spanning"},
    {"object", "global::spanning"},
    {"indices", Json::array({"0"})},
    {"shape", Json::array({1})},
    {"elem_size", 8},
    {"op", "store"},
  };
  const Json metadata = {
    {"global::flag", {{"kind", "scalar"}, {"elem_size", 4}}},
    {"global::spanning",
     {{"kind", "array"}, {"shape", Json::array({1})}, {"elem_size", 8}}},
  };
  const auto raw = task_module(Json::array({scalar, spanning}), metadata);
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::flag"] = {0x1000, 4};
  addresses.objects["global::spanning"] = {0x101c, 8};

  const auto resolved = yarda::resolved_block_traces(raw, addresses);
  const auto mapped =
    yarda::mapped_block_traces(raw, yarda::CacheGeometry{32, 8, 2}, addresses);

  EXPECT_TRUE(resolved.coverage.complete());
  EXPECT_EQ(resolved.coverage.source_accesses, 2U);
  EXPECT_EQ(resolved.coverage.resolved_accesses, 2U);
  EXPECT_EQ(resolved.coverage.rejected_accesses, 0U);
  EXPECT_EQ(resolved.coverage.emitted_line_references, 0U);
  EXPECT_TRUE(mapped.coverage.complete());
  EXPECT_EQ(mapped.coverage.source_accesses, 2U);
  EXPECT_EQ(mapped.coverage.resolved_accesses, 2U);
  EXPECT_EQ(mapped.coverage.rejected_accesses, 0U);
  EXPECT_EQ(mapped.coverage.emitted_line_references, 3U);
  ASSERT_EQ(mapped.traces.size(), 1U);
  EXPECT_EQ(mapped.traces[0].accesses.size(), 3U);
  EXPECT_EQ(mapped.traces[0].accesses[0].operation,
            yarda::AccessOperation::Load);
  EXPECT_EQ(mapped.traces[0].accesses[1].source_access_ordinal, 1U);
  EXPECT_EQ(mapped.traces[0].accesses[2].line_span_ordinal, 1U);
}

}  // namespace
