#pragma once

#include <array>
#include <gtest/gtest.h>

#include "yarda/cache/hierarchy_result_json.hpp"
#include "yarda/elf/object_addresses.hpp"

namespace yarda::test::e2e
{
/** @brief Immutable invocation paths, assigned once before Google Test starts. */
extern std::array<std::string, 6> input_paths;

/** @brief Load each test's own inputs and independent source expectations. */
class GeneratedArtifacts : public testing::Test
{
protected:
  nlohmann::json raw, golden, result, events;
  ObjectAddressModel objects;
  HierarchyResultMetadata metadata;
  ResolvedTaskTraceResult expected;
  std::string result_bytes, event_bytes;

  void SetUp() override;
};
} // namespace yarda::test::e2e
