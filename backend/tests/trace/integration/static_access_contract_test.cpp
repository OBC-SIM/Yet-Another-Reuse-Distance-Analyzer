#include <algorithm>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <map>

#include "cache/streaming/streaming_hierarchy_test_support.hpp"
#include "yarda/elf/object_addresses.hpp"
#include "yarda/trace/resolution_error.hpp"

namespace yarda::test
{
namespace
{

class StaticAccessContract : public testing::TestWithParam<const char *>
{
protected:
  nlohmann::json raw;
  ObjectAddressModel objects;

  void loadFixture(const std::string & suffix = "")
  {
    const auto stem =
      std::string(YARDA_STATIC_FIXTURE_DIR) + "/" + GetParam() + suffix;
    std::ifstream input(stem + "_ape.json");
    ASSERT_TRUE(input.good());
    input >> raw;
    objects = build_elf_object_addresses(parse_elf_data_regions(stem + ".elf"));
  }

  void SetUp() override { loadFixture(); }
};

TEST_P(StaticAccessContract, MatchesSourceAccessOrderAndElfAddresses)
{
  // Expected indices come from the C loops, independently of emitted MAP IVs.
  const std::map<std::string, std::vector<uint64_t>> expected{
    {"pointer_forward", {0, 1, 2, 3}},
    {"pointer_reverse", {6, 4, 2}},
    {"pointer_offset", {2, 3, 4}},
    {"pointer_nonunit", {0, 1, 2}},
    {"mixed_inductions", {0, 0, 2, 1, 4, 2}},
    {"descending_inductions", {7, 2, 4, 3, 1, 4}},
    {"nested_inductions", {0, 1, 8, 9, 10, 11}},
    {"not_equal_forward", {0, 2, 4}},
    {"not_equal_reverse", {6, 4, 2}},
    {"scalar_actual", {0}},
    {"pointer_actual", {0, 1, 2}},
    {"pointer_row", {3, 4, 5}},
    {"pointer_nested_carry", {1, 2, 3, 4, 5, 6}},
    {"pointer_nested_reverse", {7, 6, 5, 4, 3, 2}}};
  std::map<std::string, std::vector<ResolvedAccess>> observed;
  const auto result = stream_resolved_task_accesses(
    raw, objects,
    {[](const std::string &, uint64_t excluded) { EXPECT_EQ(excluded, 0U); },
     [&](const std::string & task, const ResolvedAccess & access) {
       observed[task].push_back(access);
     },
     [](const std::string &, const TraceCoverage & coverage) {
       EXPECT_TRUE(coverage.complete());
     }});
  ASSERT_EQ(observed.size(), expected.size());
  uint64_t total = 0;
  for (const auto & [task, indices] : expected)
  {
    SCOPED_TRACE(task);
    const auto & actual = observed.at(task);
    ASSERT_EQ(actual.size(), indices.size());
    const auto object = task == "scalar_actual" ? "global::scalar"
                        : task == "pointer_row" ? "global::matrix"
                                                : "global::storage";
    for (std::size_t i = 0; i < indices.size(); ++i)
    {
      EXPECT_EQ(actual[i].object_id, object);
      EXPECT_EQ(actual[i].object_byte_offset, 4 * indices[i]);
      EXPECT_EQ(actual[i].linked_byte_address,
                objects.objects.at(object).base + 4 * indices[i]);
      EXPECT_EQ(actual[i].access_size, 4U);
      EXPECT_EQ(actual[i].source_access_ordinal, i);
      EXPECT_EQ(actual[i].operation, AccessOperation::Store);
    }
    total += indices.size();
  }
  EXPECT_EQ(result.coverage.source_accesses, total);
  EXPECT_TRUE(result.coverage.complete());
}

TEST_P(StaticAccessContract, PreservesCacheResultsAgainstIndependentOracles)
{
  streaming::expect_stream_parity(raw, objects,
                                  streaming::make_batch_hierarchy());
}

INSTANTIATE_TEST_SUITE_P(CompilerDebugModes, StaticAccessContract,
                         testing::Values("debug", "nodebug"));

class AccessWidthContract : public StaticAccessContract
{
protected:
  void SetUp() override { loadFixture("_widths"); }

  nlohmann::json root(const std::string & name)
  {
    auto selected = raw;
    selected["functions"] = nlohmann::json::array();
    bool found = false;
    for (const auto & function : raw.at("functions"))
    {
      const auto & annotations = function.at("annotations");
      const bool requested = function.at("function") == name;
      if (requested || std::find(annotations.begin(), annotations.end(),
                                 "ape.inline") != annotations.end())
        selected["functions"].push_back(function);
      found |= requested;
    }
    EXPECT_TRUE(found);
    return selected;
  }

  struct ExpectedAccess
  {
    uint64_t offset;
    uint64_t width;
    AccessOperation operation = AccessOperation::Store;
  };

  void expectAccesses(const char * task, const char * object,
                      const std::vector<ExpectedAccess> & expected)
  {
    std::vector<ResolvedAccess> observed;
    const auto result = stream_resolved_task_accesses(
      root(task), objects,
      {[&](const std::string & name, uint64_t excluded) {
         EXPECT_EQ(name, task);
         EXPECT_EQ(excluded, 0U);
       },
       [&](const std::string &, const ResolvedAccess & access) {
         observed.push_back(access);
       },
       [](const std::string &, const TraceCoverage & coverage) {
         EXPECT_TRUE(coverage.complete());
       }});
    ASSERT_EQ(observed.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      SCOPED_TRACE(i);
      EXPECT_EQ(observed[i].object_id, object);
      EXPECT_EQ(observed[i].object_byte_offset, expected[i].offset);
      EXPECT_EQ(observed[i].linked_byte_address,
                objects.objects.at(object).base + expected[i].offset);
      EXPECT_EQ(observed[i].access_size, expected[i].width);
      EXPECT_EQ(observed[i].operation, expected[i].operation);
      EXPECT_EQ(observed[i].source_access_ordinal, i);
    }
    EXPECT_EQ(result.coverage.source_accesses, expected.size());
    EXPECT_TRUE(result.coverage.complete());
  }
};

TEST_P(AccessWidthContract, PreservesScalarCastMemoryWidth)
{
  expectAccesses("scalar_cast_width", "global::bytes", {{0, 4}});
}

TEST_P(AccessWidthContract, PreservesMixedWidthsAndOperationsOnOneFormal)
{
  expectAccesses("mixed_access_widths", "global::bytes",
                 {{0, 1}, {0, 4}, {0, 1, AccessOperation::Load}, {1, 1}});
}

TEST_P(AccessWidthContract, PreservesMatchingRowStride)
{
  // rows[1][1] is the fifth unsigned int in the C object.
  expectAccesses("matching_row_layout", "global::rows", {{16, 4}});
}

TEST_P(AccessWidthContract, PreservesStructFieldOffsetAndLeafWidth)
{
  for (const auto & function : raw.at("functions"))
    if (function.at("function") == "field_helper")
      EXPECT_FALSE(function.at("body").at(0).contains("elem_size"));
  // Clang's target ABI places the four-byte member after three padding bytes.
  expectAccesses("matching_field_layout", "global::record", {{4, 4}});
}

TEST_P(AccessWidthContract, PreservesRuntimeAccessesUntilElfRejection)
{
  for (const auto * task :
       {"runtime_cast_before_gep", "runtime_cast_after_gep",
        "runtime_pointer_induction", "runtime_indexed_induction"})
  {
    SCOPED_TRACE(task);
    auto selected = root(task);
    std::vector<nlohmann::json *> accesses;
    std::function<void(nlohmann::json &)> visit = [&](nlohmann::json & body) {
      for (auto & node : body)
      {
        const auto type = node.value("type", "");
        if (type == "Scalar" || type == "Array") accesses.push_back(&node);
        if (node.contains("body")) visit(node["body"]);
      }
    };
    for (auto & function : selected["functions"])
      if (function.at("function") == task) visit(function["body"]);
    ASSERT_EQ(accesses.size(), 1U);
    auto & access = *accesses.front();
    EXPECT_FALSE(access.contains("object"));
    EXPECT_EQ(access.at("elem_size"), 4);
    EXPECT_EQ(access.at("op"), "store");
    for (const bool collide_with_global : {false, true})
    {
      SCOPED_TRACE(collide_with_global);
      // Display names must never manufacture ELF identity for runtime storage.
      if (collide_with_global) access["name"] = "bytes";
      try
      {
        stream_resolved_task_accesses(
          selected, objects,
          {[](const std::string &, uint64_t excluded) {
             // Legacy extraction omits unannotated malloc/free calls from MAP.
             EXPECT_EQ(excluded, 0U);
           },
           [](const std::string &, const ResolvedAccess &) {
             ADD_FAILURE() << "runtime storage emitted a linked address";
           },
           [](const std::string &, const TraceCoverage &) {
             ADD_FAILURE() << "runtime storage completed analysis";
           }});
        FAIL() << "runtime storage was accepted";
      }
      catch (const ResolutionError & error)
      {
        EXPECT_EQ(error.category(), ResolutionCategory::Unsupported);
        EXPECT_EQ(error.task_id(), task);
        EXPECT_EQ(error.source_access_ordinal(), 0U);
        EXPECT_TRUE(error.object_id().empty());
        EXPECT_EQ(error.coverage().source_accesses, 1U);
        EXPECT_EQ(error.coverage().resolved_accesses, 0U);
        EXPECT_EQ(error.coverage().rejected_accesses, 1U);
        EXPECT_FALSE(error.coverage().complete());
      }
    }
  }
}

TEST_P(AccessWidthContract, RejectsIncompatibleArrayBindings)
{
  for (const auto & [task, diagnostic] : std::map<std::string, std::string>{
         {"incompatible_gep_width", "incompatible element size"},
         {"incompatible_pointer_width", "incompatible element size"},
         {"incompatible_row_layout", "incompatible row dimensions"}})
  {
    SCOPED_TRACE(task);
    try
    {
      stream_resolved_task_accesses(
        root(task), objects,
        {[](const std::string &, uint64_t) {},
         [](const std::string &, const ResolvedAccess &) {
           ADD_FAILURE() << "incompatible layout emitted an access";
         },
         [](const std::string &, const TraceCoverage &) {}});
      FAIL() << "incompatible layout was accepted";
    }
    catch (const std::invalid_argument & error)
    {
      EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(CompilerDebugModes, AccessWidthContract,
                         testing::Values("debug", "nodebug"));

}  // namespace
}  // namespace yarda::test
