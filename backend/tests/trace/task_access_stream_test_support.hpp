#pragma once

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

#include "yarda/trace/resolved_mapping.hpp"
#include "yarda/trace/task_access_stream.hpp"

namespace yarda::test::stream
{

using Json = nlohmann::json;

inline Json access(std::string object = "global::A", std::string index = "0",
                   std::string operation = "load")
{
  return {{"type", "Array"},
          {"name", object},
          {"object", std::move(object)},
          {"indices", Json::array({std::move(index)})},
          {"op", std::move(operation)}};
}

inline Json function(std::string name, Json body,
                     std::string role = "ape.analyze")
{
  return {{"function", std::move(name)},
          {"body", std::move(body)},
          {"annotations", Json::array({std::move(role)})}};
}

inline Json loop(std::int64_t bound, Json body, std::string variable = "i",
                 std::int64_t start = 0, std::int64_t step = 1)
{
  return {{"type", "Loop"}, {"var", std::move(variable)},
          {"start", start}, {"bound", bound},
          {"step", step},   {"body", std::move(body)}};
}

inline Json call(std::string callee)
{
  return {
    {"type", "Call"}, {"callee", std::move(callee)}, {"args", Json::array()}};
}

inline Json module(Json functions, std::uint64_t element_size = 4)
{
  Json metadata = Json::object();
  for (const auto * object : {"global::A", "global::B", "global::C"})
  {
    metadata[object] = {{"kind", "array"},
                        {"shape", Json::array({64})},
                        {"elem_size", element_size}};
  }
  return {{"schema_version", 2},
          {"metadata", {{"objects", metadata}}},
          {"functions", std::move(functions)}};
}

inline ObjectAddressModel addresses(std::uint64_t element_size = 4)
{
  ObjectAddressModel result;
  result.objects["global::A"] = {0x101c, 64 * element_size};
  result.objects["global::B"] = {0x2000, 64 * element_size};
  result.objects["global::C"] = {0x3000, 64 * element_size};
  return result;
}

/** @brief Keep callback observations for small, explicitly bounded fixtures. */
struct Collector
{
  ResolvedTaskTraceResult result;
  std::vector<std::string> notifications;

  TaskAccessSink sink()
  {
    return {
      [this](const std::string & id, std::uint64_t excluded) {
        notifications.push_back("begin:" + id);
        result.tasks.push_back({id, {}, {}, excluded});
      },
      [this](const std::string & id, const ResolvedAccess & value) {
        notifications.push_back("access:" + id + ":" +
                                std::to_string(value.source_access_ordinal));
        result.tasks.back().accesses.push_back(value);
      },
      [this](const std::string & id, const TraceCoverage & coverage) {
        notifications.push_back("end:" + id);
        result.tasks.back().coverage = coverage;
      },
    };
  }

  void complete(const TaskAccessStreamResult & summary)
  {
    result.coverage = summary.coverage;
    result.excluded_opaque_call_sites = summary.excluded_opaque_call_sites;
  }
};

inline TaskAccessSink discard_sink()
{
  return {
    [](const std::string &, std::uint64_t) {},
    [](const std::string &, const ResolvedAccess &) {},
    [](const std::string &, const TraceCoverage &) {},
  };
}

inline void expect_coverage(const TraceCoverage & actual,
                            const TraceCoverage & expected)
{
  EXPECT_EQ(actual.source_accesses, expected.source_accesses);
  EXPECT_EQ(actual.resolved_accesses, expected.resolved_accesses);
  EXPECT_EQ(actual.rejected_accesses, expected.rejected_accesses);
  EXPECT_EQ(actual.emitted_line_references, expected.emitted_line_references);
  EXPECT_EQ(actual.complete(), expected.complete());
}

inline void expect_access(const ResolvedAccess & actual,
                          const ResolvedAccess & expected)
{
  EXPECT_EQ(actual.object_id, expected.object_id);
  EXPECT_EQ(actual.object_byte_offset, expected.object_byte_offset);
  EXPECT_EQ(actual.access_size, expected.access_size);
  EXPECT_EQ(actual.linked_byte_address, expected.linked_byte_address);
  EXPECT_EQ(actual.address_basis, expected.address_basis);
  EXPECT_EQ(actual.operation, expected.operation);
  EXPECT_EQ(actual.source_access_ordinal, expected.source_access_ordinal);
}

inline void expect_tasks(const ResolvedTaskTraceResult & actual,
                         const ResolvedTaskTraceResult & expected)
{
  expect_coverage(actual.coverage, expected.coverage);
  EXPECT_EQ(actual.excluded_opaque_call_sites,
            expected.excluded_opaque_call_sites);
  ASSERT_EQ(actual.tasks.size(), expected.tasks.size());
  for (std::size_t task = 0; task < actual.tasks.size(); ++task)
  {
    SCOPED_TRACE(task);
    const auto & left = actual.tasks[task];
    const auto & right = expected.tasks[task];
    EXPECT_EQ(left.task_id, right.task_id);
    EXPECT_EQ(left.excluded_opaque_call_sites,
              right.excluded_opaque_call_sites);
    expect_coverage(left.coverage, right.coverage);
    ASSERT_EQ(left.accesses.size(), right.accesses.size());
    for (std::size_t index = 0; index < left.accesses.size(); ++index)
    {
      SCOPED_TRACE(index);
      expect_access(left.accesses[index], right.accesses[index]);
    }
  }
}

inline void expect_mapping(const CacheLineMapping & actual,
                           const CacheLineMapping & expected)
{
  EXPECT_EQ(actual.object_id, expected.object_id);
  EXPECT_EQ(actual.object_byte_offset, expected.object_byte_offset);
  EXPECT_EQ(actual.address_basis, expected.address_basis);
  EXPECT_EQ(actual.decoded.address, expected.decoded.address);
  EXPECT_EQ(actual.decoded.block_number, expected.decoded.block_number);
  EXPECT_EQ(actual.decoded.set_index, expected.decoded.set_index);
  EXPECT_EQ(actual.decoded.tag, expected.decoded.tag);
  EXPECT_EQ(actual.decoded.line_offset, expected.decoded.line_offset);
  EXPECT_EQ(actual.source_object_byte_offset,
            expected.source_object_byte_offset);
  EXPECT_EQ(actual.source_access_size, expected.source_access_size);
  EXPECT_EQ(actual.source_linked_byte_address,
            expected.source_linked_byte_address);
  EXPECT_EQ(actual.operation, expected.operation);
  EXPECT_EQ(actual.source_access_ordinal, expected.source_access_ordinal);
  EXPECT_EQ(actual.line_span_ordinal, expected.line_span_ordinal);
}

}  // namespace yarda::test::stream
