#pragma once

#include "../../src/trace/prepared_layout.hpp"
#include "prepared_trace_test_support.hpp"

namespace yarda::test::prepared_access
{

using namespace ::yarda::test::prepared;

/** @brief A padded cell array with independently calculable 48/16-byte strides.
 */
inline Json structured_module()
{
  auto raw = module(Json::array());
  raw["metadata"]["objects"]["global::A"] = {{"kind", "array"},
                                             {"shape", {2, 3}},
                                             {"elem_type", "Cell"},
                                             {"elem_size", 16}};
  raw["metadata"]["structs"]["Cell"] = {{"size", 16},
                                        {"fields", Json::array({
                                                     {{"name", "x"},
                                                      {"index", 0},
                                                      {"offset", 0},
                                                      {"size", 4},
                                                      {"kind", "scalar"},
                                                      {"elem_size", 4}},
                                                     {{"name", "y"},
                                                      {"index", 1},
                                                      {"offset", 8},
                                                      {"size", 8},
                                                      {"kind", "scalar"},
                                                      {"elem_size", 8}},
                                                   })}};
  return raw;
}

/** @brief Select A[i][j].y, including the eight-byte padding displacement. */
inline Json structured_access()
{
  auto node = access("global::A", "i");
  node["indices"] = {"i", "j"};
  node["access_path"] = Json::array({
    {{"kind", "index"}, {"value", "i"}},
    {{"kind", "index"}, {"value", "j"}},
    {{"kind", "field"}, {"name", "y"}, {"index", 1}},
  });
  return node;
}

/** @brief Check exact layout bytes without consulting a second resolver. */
inline void expect_bytes(const std::optional<detail::ByteAccess> & actual,
                         std::int64_t offset, std::int64_t width)
{
  ASSERT_TRUE(actual);
  EXPECT_EQ(actual->offset, offset);
  EXPECT_EQ(actual->size, width);
}

/** @brief Require the original structured diagnostic rather than any throw. */
template <typename Function>
void expect_layout_error(Function action, const std::string & reason)
{
  try
  {
    action();
    FAIL() << "expected structured rejection";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_EQ(error.what(), "structured access " + reason + ": global::A");
  }
}

/** @brief Build one selected task while preserving supplied layout metadata. */
inline Json kernel(Json body, Json raw = module(Json::array()))
{
  raw["functions"] = Json::array({function("kernel", std::move(body))});
  return raw;
}

/** @brief Require exact rejection provenance and no callbacks after failure. */
inline void expect_resolution_failure(
  const Json & raw, const ObjectAddressModel & objects,
  const std::string & reason, std::uint64_t ordinal = 0,
  ResolutionCategory category = ResolutionCategory::Unresolved)
{
  Collector collector;
  try
  {
    stream_resolved_task_accesses(raw, objects, collector.sink());
    FAIL() << "expected resolution failure";
  }
  catch (const ResolutionError & error)
  {
    EXPECT_EQ(error.category(), category);
    EXPECT_EQ(error.task_id(), "kernel");
    EXPECT_EQ(error.object_id(), "global::A");
    EXPECT_EQ(error.source_access_ordinal(), ordinal);
    expect_coverage(error.coverage(), {ordinal + 1, ordinal, 1, 0});
    const auto label = category == ResolutionCategory::Unsupported ? "unsupport"
                                                                     "ed"
                                                                   : "unresolve"
                                                                     "d";
    EXPECT_EQ(error.what(),
              std::string(label) + " task access [task=kernel, ordinal=" +
                std::to_string(ordinal) + ", object=global::A]: " + reason);
  }
  std::vector<std::string> expected{"begin:kernel"};
  for (std::uint64_t i = 0; i < ordinal; ++i)
    expected.push_back("access:kernel:" + std::to_string(i));
  EXPECT_EQ(collector.notifications, expected);
}

}  // namespace yarda::test::prepared_access
