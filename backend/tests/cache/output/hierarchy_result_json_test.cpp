#include <fstream>
#include <iterator>
#include <limits>

#include "artifact_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;
namespace stream = yarda::test::streaming;

TEST(HierarchyResultJsonTest, MatchesCompleteGoldenDocumentBytes)
{
  std::ifstream input(YARDA_RESULT_GOLDEN_PATH);
  ASSERT_TRUE(input);
  const std::string expected{std::istreambuf_iterator<char>(input), {}};
  EXPECT_EQ(hierarchy_result_json(metadata(), result()).dump(2) + "\n",
            expected);
}

TEST(HierarchyResultJsonTest, KeepsNumericHistogramOrderAndAllDistances)
{
  const auto payload = hierarchy_result_json(metadata(), histogram_result());
  const auto & histogram = payload["tasks"][0]["l1"]["csrd_histogram"];
  EXPECT_EQ(histogram.dump(), "{\"0\":1,\"2\":2,\"10\":3}");
  EXPECT_EQ(payload["tasks"][0]["l1"]["cold_misses"], 11);
}

TEST(HierarchyResultJsonTest, PreservesEveryLargeHistogramKeyInNumericOrder)
{
  const auto payload =
      hierarchy_result_json(metadata(), large_histogram_result());
  for (const auto * level : {"l1", "llc"})
  {
    const auto & histogram = payload["tasks"][0][level]["csrd_histogram"];
    std::uint64_t distance = std::string(level) == "l1" ? 0 : 1;
    ASSERT_EQ(histogram.size(), 20000U - distance);
    for (auto entry = histogram.begin(); entry != histogram.end(); ++entry)
    {
      EXPECT_EQ(entry.key(), std::to_string(distance++));
      EXPECT_TRUE(entry.value().is_number_unsigned());
      EXPECT_EQ(entry.value(), 1);
    }
    EXPECT_EQ(distance, 20000U);
  }
}

TEST(HierarchyResultJsonTest, CountsCrossLineStoreAsOneSourceAndTwoReferences)
{
  const auto raw = stream::stream::module(
      stream::Json::array({stream::function(
          "wide", {stream::access("global::A", "0", "store")})}),
      8);
  const auto value = analyze_streaming_hierarchy(
      raw, stream::stream::addresses(8), metadata().hierarchy);
  const auto task = hierarchy_result_json(metadata(), value)["tasks"][0];
  EXPECT_EQ(task["source_accesses"], 1);
  EXPECT_EQ(task["ma"], 2);
  EXPECT_EQ(task["ehc_l1"], 0);
  EXPECT_EQ(task["ehc_llc"], 0);
  EXPECT_EQ(task["amc"], 2);
  EXPECT_EQ(task["mr"], 1.0);
  EXPECT_EQ(task["l1"]["lookups"], 2);
  EXPECT_EQ(task["llc"]["cold_misses"], 2);
  EXPECT_EQ(task["coverage"]["source_accesses"], 1);
  EXPECT_EQ(task["coverage"]["emitted_line_references"], 2);
  EXPECT_EQ(task["coverage"]["complete"], true);
}

TEST(HierarchyResultJsonTest, RetainsEmptyTasksInLatOrderWithNullRatios)
{
  auto region = stream::function("scope", stream::Json::array());
  region["analysis_scope"] = {{"kind", "region"}, {"name", "APE_ANALYZE"}};
  const auto raw = stream::byte_module(
      stream::Json::array({stream::function("z", stream::Json::array()), region,
                           stream::function("a", stream::Json::array())}));
  const auto analyzed = analyze_streaming_hierarchy(
      raw, stream::byte_addresses(), metadata().hierarchy);
  const auto tasks = hierarchy_result_json(metadata(), analyzed)["tasks"];
  ASSERT_EQ(tasks.size(), 3U);
  EXPECT_EQ(tasks[0]["task_id"], "z");
  EXPECT_EQ(tasks[1]["task_id"], "region:5:scope:APE_ANALYZE");
  EXPECT_EQ(tasks[2]["task_id"], "a");
  for (const auto & task : tasks)
  {
    EXPECT_TRUE(task["hr_l1"].is_null());
    EXPECT_TRUE(task["hr_llc"].is_null());
    EXPECT_TRUE(task["mr"].is_null());
    EXPECT_EQ(task["ma"], 0);
  }
}

TEST(HierarchyResultJsonTest, PreservesCountsBeyondSignedAndDoublePrecision)
{
  const auto count = std::numeric_limits<std::uint64_t>::max();
  auto value = result();
  auto & task = value.tasks[0];
  task.source_accesses = task.modeled_accesses = count;
  task.l1 = {count, count - 1, 1, 1, 0, 1, {{0, count - 1}}};
  task.llc = {1, 0, 1, 1, 0, 1, {}};
  task.ehc_l1 = count - 1;
  task.ehc_llc = 0;
  task.all_cache_misses = 1;
  task.hr_l1 = 1.0;
  task.hr_llc = 0.0;
  task.miss_ratio = 1.0 / static_cast<double>(count);
  task.coverage = value.coverage = {count, count, 0, count};
  const auto payload = hierarchy_result_json(metadata(), value);
  EXPECT_TRUE(payload["tasks"][0]["ma"].is_number_unsigned());
  EXPECT_EQ(payload["tasks"][0]["ma"].dump(), "18446744073709551615");
  EXPECT_EQ(payload["tasks"][0]["l1"]["csrd_histogram"]["0"].dump(),
            "18446744073709551614");
}

TEST(HierarchyResultJsonTest, DiagnosticMetadataDoesNotChangeResultBytes)
{
  auto value = result();
  const auto before = hierarchy_result_json(metadata(), value).dump();
  value.event_delivery = {1, true};
  EXPECT_EQ(hierarchy_result_json(metadata(), value).dump(), before);
}

TEST(HierarchyResultJsonTest, SerializesElf32AndEffectiveGeometryOnly)
{
  auto info = metadata();
  info.elf_address_size = 4;
  info.elf_machine = 2;
  const auto payload = hierarchy_result_json(info, result());
  EXPECT_EQ(payload["inputs"]["elf_class"], "ELF32");
  EXPECT_EQ(payload["cache_hierarchy"]["levels"][0]["size_bytes"], 64);
  EXPECT_FALSE(
      payload["cache_hierarchy"]["levels"][0].contains("write_policy"));
  EXPECT_FALSE(
      payload["cache_hierarchy"]["levels"][0].contains("delay_cycles"));
}

} // namespace
