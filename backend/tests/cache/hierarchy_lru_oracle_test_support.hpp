#pragma once

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>

#include "hierarchy_lru_oracle.hpp"
#include "yarda/access_operation.hpp"

namespace yarda
{
namespace test
{
namespace support
{

inline CacheLineMapping
make_mapping(std::uint64_t block, const CacheGeometry & geometry,
             std::uint64_t source_access_ordinal, std::uint64_t line_offset = 0,
             AccessOperation operation = AccessOperation::Load)
{
  CacheLineMapping mapping;
  mapping.object_id = "global::oracle";
  mapping.address_basis = AddressBasis::Absolute;
  mapping.decoded =
    decode_cache_address(block * geometry.line_size + line_offset, geometry);
  mapping.operation = operation;
  mapping.source_access_ordinal = source_access_ordinal;
  return mapping;
}

inline MappedTaskTrace make_task(std::initializer_list<std::uint64_t> blocks,
                                 const CacheGeometry & geometry,
                                 const std::string & id = "kernel")
{
  MappedTaskTrace result;
  result.task_id = id;
  for (const auto block : blocks)
  {
    result.accesses.push_back(
      make_mapping(block, geometry, result.accesses.size()));
  }
  return result;
}

inline void expect_contract_invariants(const OracleTaskHierarchy & result,
                                       std::size_t input_references)
{
  SCOPED_TRACE("task_id=" + result.task_id);
  EXPECT_EQ(result.events.size(), input_references);
  EXPECT_EQ(result.l1.lookups, input_references);
  EXPECT_EQ(result.l1.lookups, result.l1.hits + result.l1.misses);
  EXPECT_EQ(result.l1.misses,
            result.l1.cold_misses + result.l1.replacement_misses);
  EXPECT_EQ(result.llc.lookups, result.l1.misses);
  EXPECT_EQ(result.llc.lookups, result.llc.hits + result.llc.misses);
  EXPECT_EQ(result.llc.misses,
            result.llc.cold_misses + result.llc.replacement_misses);
  EXPECT_EQ(result.ehc_l1, result.l1.hits);
  EXPECT_EQ(result.ehc_llc, result.llc.hits);
  EXPECT_EQ(result.all_cache_misses, result.llc.misses);
  EXPECT_EQ(result.ehc_l1 + result.ehc_llc + result.all_cache_misses,
            result.l1.lookups);
  EXPECT_EQ(result.llc.lookups, result.ehc_llc + result.all_cache_misses);
}

}  // namespace support
}  // namespace test
}  // namespace yarda
