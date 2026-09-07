#pragma once

#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <utility>

#include "hierarchy_model_test_support.hpp"
#include "yarda/cache/hierarchy_analysis.hpp"

namespace yarda::test::support
{

/**
 * @brief Select a small supported hierarchy for eviction witnesses.
 *
 * @param l1 L1 geometry with the same line size as llc.
 * @param llc LLC geometry.
 * @return Snapshot obtained through the production configuration selector.
 */
inline AnalysisHierarchy make_batch_hierarchy(CacheGeometry l1 = {32, 8, 2},
                                              CacheGeometry llc = {32, 32, 4})
{
  auto config = make_analysis_config(l1.line_size);
  config.caches[0].size_bytes = l1.line_size * l1.line_count;
  config.caches[0].associativity = l1.associativity;
  config.caches[1].size_bytes = llc.line_size * llc.line_count;
  config.caches[1].line_size = llc.line_size;
  config.caches[1].associativity = llc.associativity;
  return select_analysis_hierarchy(config);
}

/**
 * @brief Append a source range and maintain geometry-independent coverage.
 *
 * @param task Task to extend with its next source ordinal.
 * @param address Absolute linked first byte.
 * @param size Positive byte count.
 * @param operation Source load or store.
 * @param object Canonical source identity.
 * @param offset Object-relative first byte.
 * @return Nothing.
 */
inline void append_batch_access(
  ResolvedTaskTrace & task, std::uint64_t address, std::uint64_t size = 1,
  AccessOperation operation = AccessOperation::Load,
  std::string object = "global::batch", std::uint64_t offset = 0)
{
  task.accesses.push_back({std::move(object), offset, size, address,
                           AddressBasis::Absolute, operation,
                           task.coverage.source_accesses});
  ++task.coverage.source_accesses;
  ++task.coverage.resolved_accesses;
}

/**
 * @brief Build a trace from literal absolute byte addresses.
 *
 * @param addresses Ordered one-byte accesses.
 * @param id Non-empty task identity.
 * @return Independent task with complete source coverage.
 */
inline ResolvedTaskTrace make_batch_task(
  std::initializer_list<std::uint64_t> addresses, std::string id = "kernel")
{
  ResolvedTaskTrace task;
  task.task_id = std::move(id);
  for (const auto address : addresses) append_batch_access(task, address);
  return task;
}

/**
 * @brief Combine valid fixture tasks without mapping or analyzing them.
 *
 * @param tasks Ordered independent tasks.
 * @return Resolved module with matching aggregate coverage.
 */
inline ResolvedTaskTraceResult
batch_input(std::initializer_list<ResolvedTaskTrace> tasks)
{
  ResolvedTaskTraceResult result;
  result.tasks = tasks;
  for (const auto & task : tasks)
  {
    result.coverage.source_accesses += task.coverage.source_accesses;
    result.coverage.resolved_accesses += task.coverage.resolved_accesses;
    result.coverage.rejected_accesses += task.coverage.rejected_accesses;
    result.coverage.emitted_line_references +=
      task.coverage.emitted_line_references;
    result.excluded_opaque_call_sites += task.excluded_opaque_call_sites;
  }
  return result;
}

/**
 * @brief Compare the full provenance that survives a level remapping.
 *
 * @param actual Mapping to check.
 * @param expected Independently mapped input row.
 * @return Nothing; reports differences through Google Test.
 */
inline void expect_batch_provenance(const CacheLineMapping & actual,
                                    const CacheLineMapping & expected)
{
  EXPECT_EQ(actual.object_id, expected.object_id);
  EXPECT_EQ(actual.object_byte_offset, expected.object_byte_offset);
  EXPECT_EQ(actual.address_basis, expected.address_basis);
  EXPECT_EQ(actual.decoded.address, expected.decoded.address);
  EXPECT_EQ(actual.source_object_byte_offset,
            expected.source_object_byte_offset);
  EXPECT_EQ(actual.source_access_size, expected.source_access_size);
  EXPECT_EQ(actual.source_linked_byte_address,
            expected.source_linked_byte_address);
  EXPECT_EQ(actual.operation, expected.operation);
  EXPECT_EQ(actual.source_access_ordinal, expected.source_access_ordinal);
  EXPECT_EQ(actual.line_span_ordinal, expected.line_span_ordinal);
}

/**
 * @brief Project L1 observations for existing cache-level oracle assertions.
 *
 * @param task Owned batch events in L1 reference order.
 * @return Copies of L1 payload and the reported L1 summary.
 */
inline BatchCacheLevelResult
batch_l1_result(const BatchTaskHierarchyResult & task)
{
  BatchCacheLevelResult result;
  result.summary = task.summary.l1;
  for (const auto & event : task.events)
  {
    result.mappings.push_back(event.l1_mapping);
    result.accesses.push_back(event.l1);
  }
  return result;
}

/**
 * @brief Project present LLC observations without consulting FSL decisions.
 *
 * @param task Owned batch events in L1 reference order.
 * @return Copies of present LLC payload and its summary; a mismatched pair
 * of optionals also reports a Google Test failure.
 */
inline BatchCacheLevelResult
batch_llc_result(const BatchTaskHierarchyResult & task)
{
  BatchCacheLevelResult result;
  result.summary = task.summary.llc;
  for (const auto & event : task.events)
  {
    EXPECT_EQ(event.llc_mapping.has_value(), event.llc.has_value());
    if (event.llc && event.llc_mapping)
    {
      result.mappings.push_back(*event.llc_mapping);
      result.accesses.push_back(*event.llc);
    }
  }
  return result;
}

}  // namespace yarda::test::support
