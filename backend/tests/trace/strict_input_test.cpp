#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/resolution_error.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace
{

using Json = nlohmann::json;

Json array_access(std::string operation = "load")
{
  return {
    {"type", "Array"},
    {"name", "A"},
    {"object", "global::A"},
    {"indices", Json::array({"0"})},
    {"shape", Json::array({1})},
    {"elem_size", 4},
    {"op", std::move(operation)},
  };
}

Json module_with(Json access, Json metadata = Json::object())
{
  return {
    {"metadata", {{"objects", std::move(metadata)}}},
    {"functions", Json::array({{{"function", "kernel"},
                                {"annotations", Json::array({"ape.analyze"})},
                                {"body", Json::array({std::move(access)})}}})},
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

template <typename Action>
void expect_normalized_malformed_lat(Action action)
{
  try
  {
    action();
    FAIL() << "expected malformed LAT rejection";
  }
  catch (const nlohmann::json::exception & error)
  {
    FAIL() << "JSON exception escaped public API: " << error.what();
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_NE(std::string(error.what()).find("malformed LAT input:"),
              std::string::npos);
  }
}

TEST(StrictInputTest, RejectsLegacyModuleWithoutObjectMetadata)
{
  const Json raw = Json::array(
    {{{"function", "kernel"}, {"body", Json::array({array_access()})}}});
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 4};

  const auto error = capture_resolution_error(
    [&]() { static_cast<void>(yarda::resolved_block_traces(raw, addresses)); });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unresolved);
  EXPECT_EQ(error->object_id(), "global::A");
}

TEST(StrictInputTest, RejectsMalformedObjectKindMetadata)
{
  const Json metadata = {
    {"global::A",
     {{"kind", Json::array({"pointer"})},
      {"shape", Json::array({1})},
      {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 4};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      module_with(array_access(), metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unresolved);
}

TEST(StrictInputTest, RejectsIndicesOnScalarAccess)
{
  const Json access = {
    {"type", "Scalar"},      {"name", "A"},
    {"object", "global::A"}, {"indices", Json::array({"runtime"})},
    {"elem_size", 4},        {"op", "load"},
  };
  const Json metadata = {
    {"global::A", {{"kind", "scalar"}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 4};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(
      yarda::resolved_block_traces(module_with(access, metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unresolved);
}

TEST(StrictInputTest, DescribesUnknownOperationAsUnsupported)
{
  const Json metadata = {
    {"global::A",
     {{"kind", "array"}, {"shape", Json::array({1})}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 4};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      module_with(array_access("atomicrmw"), metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unsupported);
  EXPECT_NE(std::string(error->what()).find("unsupported memory operation"),
            std::string::npos);
  EXPECT_NE(std::string(error->what()).find("atomicrmw"), std::string::npos);
}

TEST(StrictInputTest, DescribesUnknownObjectKindAsUnsupported)
{
  const Json metadata = {
    {"global::A",
     {{"kind", "vector"}, {"shape", Json::array({1})}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 4};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(yarda::resolved_block_traces(
      module_with(array_access(), metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unsupported);
  EXPECT_NE(std::string(error->what()).find("object kind is not recognized"),
            std::string::npos);
  EXPECT_NE(std::string(error->what()).find("vector"), std::string::npos);
}

TEST(StrictInputTest, ClassifiesJsonLayoutFailureAsUnresolved)
{
  auto access = array_access();
  access["access_path"] = Json::array(
    {{{"kind", Json::array({"field"})}, {"name", "value"}, {"index", 0}}});
  const Json metadata = {
    {"global::A", {{"kind", "struct"}, {"elem_type", "S"}, {"elem_size", 4}}},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 4};

  const auto error = capture_resolution_error([&]() {
    static_cast<void>(
      yarda::resolved_block_traces(module_with(access, metadata), addresses));
  });

  ASSERT_TRUE(error);
  EXPECT_EQ(error->category(), yarda::ResolutionCategory::Unresolved);
}

TEST(StrictInputTest, NormalizesMalformedResolvedLatException)
{
  const Json malformed = {
    {"functions", Json::array({{{"body", Json::array()}}})},
  };

  expect_normalized_malformed_lat([&]() {
    static_cast<void>(
      yarda::resolved_block_traces(malformed, yarda::ObjectAddressModel{}));
  });
}

TEST(StrictInputTest, NormalizesMalformedMappedLatException)
{
  const Json malformed = {
    {"functions", Json::array({{{"body", Json::array()}}})},
  };

  expect_normalized_malformed_lat([&]() {
    static_cast<void>(yarda::mapped_block_traces(
      malformed, yarda::CacheGeometry{64, 8, 2}, yarda::ObjectAddressModel{}));
  });
}

}  // namespace
