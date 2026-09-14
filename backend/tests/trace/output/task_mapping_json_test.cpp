#include "yarda/trace/task_mapping_json.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <stdexcept>

namespace
{

yarda::ResolvedTaskTraceResult resolved_fixture()
{
  yarda::ResolvedAccess access;
  access.object_id = "global::wide";
  access.access_size = 8;
  access.linked_byte_address = 0x101c;
  access.operation = yarda::AccessOperation::Store;

  yarda::ResolvedTaskTraceResult result;
  result.tasks = {{"kernel", {access}, {1, 1, 0, 0}, 1}};
  result.coverage = {1, 1, 0, 0};
  result.excluded_opaque_call_sites = 1;
  return result;
}

yarda::TaskMappingReportMetadata report_metadata()
{
  yarda::TaskMappingReportMetadata metadata;
  metadata.lat_path = "fixture.json";
  metadata.elf_path = "fixture.elf";
  metadata.cache_path = "cache.yaml";
  metadata.elf_image_type = yarda::ElfImageType::Executable;
  metadata.elf_address_size = 8;
  metadata.elf_machine = 62;
  metadata.cache_name = "L1D0";
  metadata.geometry = {32, 1024, 4};
  return metadata;
}

TEST(TaskMappingJsonTest, SerializesResolvedAndMappedProvenance)
{
  const auto resolved = resolved_fixture();
  const auto mapped =
    yarda::map_resolved_task_traces(resolved, report_metadata().geometry);

  const auto payload =
    yarda::task_mapping_json(report_metadata(), resolved, mapped);

  EXPECT_EQ(payload["schema_version"], 1);
  EXPECT_EQ(payload["mode"], "elf-task-mapping");
  EXPECT_EQ(payload["inputs"]["lat"], "fixture.json");
  EXPECT_EQ(payload["inputs"]["elf"], "fixture.elf");
  EXPECT_EQ(payload["elf"]["type"], "ET_EXEC");
  EXPECT_EQ(payload["elf"]["machine"], 62);
  EXPECT_EQ(payload["elf"]["address_basis"], "linked_absolute");
  EXPECT_EQ(payload["cache"]["set_count"], 256);
  EXPECT_EQ(payload["coverage"]["source_accesses"], 1);
  EXPECT_TRUE(payload["coverage"]["complete"]);
  EXPECT_EQ(payload["exclusions"]["known_non_inline_static_call_sites"], 1);

  const auto & task = payload["tasks"][0];
  EXPECT_EQ(task["task_id"], "kernel");
  EXPECT_EQ(task["resolved_accesses"][0]["operation"], "store");
  EXPECT_EQ(task["resolved_accesses"][0]["linked_byte_address"], 0x101c);
  ASSERT_EQ(task["mapped_line_references"].size(), 2U);
  EXPECT_EQ(task["mapped_line_references"][0]["line_offset"], 28);
  EXPECT_EQ(task["mapped_line_references"][0]["source_access_ordinal"], 0);
  EXPECT_EQ(task["mapped_line_references"][1]["line_span_ordinal"], 1);
  EXPECT_EQ(task["mapped_line_references"][1]["linked_byte_address"], 0x1020);
  ASSERT_EQ(payload["mappings"].size(), 2U);
  EXPECT_EQ(payload["mappings"][1]["object_byte_offset"], 4);
}

TEST(TaskMappingJsonTest, OrdersMappingsIndependentlyOfTaskOrder)
{
  auto forward = resolved_fixture();
  auto second_access = forward.tasks[0].accesses[0];
  second_access.object_id = "global::narrow";
  second_access.linked_byte_address = 0x2000;
  forward.tasks.push_back({"other", {second_access}, {1, 1, 0, 0}, 0});
  forward.coverage = {2, 2, 0, 0};
  auto reverse = forward;
  std::reverse(reverse.tasks.begin(), reverse.tasks.end());

  const auto forward_mapped =
    yarda::map_resolved_task_traces(forward, report_metadata().geometry);
  const auto reverse_mapped =
    yarda::map_resolved_task_traces(reverse, report_metadata().geometry);
  const auto forward_payload =
    yarda::task_mapping_json(report_metadata(), forward, forward_mapped);
  const auto reverse_payload =
    yarda::task_mapping_json(report_metadata(), reverse, reverse_mapped);

  EXPECT_EQ(forward_payload["mappings"], reverse_payload["mappings"]);
  EXPECT_EQ(forward_payload["mappings"][0]["object_id"], "global::narrow");
}

TEST(TaskMappingJsonTest, RejectsNonExecutableImageType)
{
  const auto resolved = resolved_fixture();
  const auto mapped =
    yarda::map_resolved_task_traces(resolved, report_metadata().geometry);
  auto metadata = report_metadata();
  metadata.elf_image_type = yarda::ElfImageType::SharedObject;

  EXPECT_THROW(yarda::task_mapping_json(metadata, resolved, mapped),
               std::invalid_argument);
}

TEST(TaskMappingJsonTest, RejectsImageRelativeAddress)
{
  auto resolved = resolved_fixture();
  resolved.tasks[0].accesses[0].address_basis =
    yarda::AddressBasis::ImageRelative;
  const auto mapped =
    yarda::map_resolved_task_traces(resolved, report_metadata().geometry);

  EXPECT_THROW(yarda::task_mapping_json(report_metadata(), resolved, mapped),
               std::invalid_argument);
}

TEST(TaskMappingJsonTest, RejectsMismatchedTaskIdentity)
{
  const auto resolved = resolved_fixture();
  auto mapped =
    yarda::map_resolved_task_traces(resolved, report_metadata().geometry);
  mapped.tasks[0].task_id = "other";

  EXPECT_THROW(yarda::task_mapping_json(report_metadata(), resolved, mapped),
               std::invalid_argument);
}

TEST(TaskMappingJsonTest, RejectsMismatchedAggregateCoverage)
{
  const auto resolved = resolved_fixture();
  auto mapped =
    yarda::map_resolved_task_traces(resolved, report_metadata().geometry);
  ++mapped.coverage.source_accesses;

  EXPECT_THROW(yarda::task_mapping_json(report_metadata(), resolved, mapped),
               std::invalid_argument);
}

}  // namespace
