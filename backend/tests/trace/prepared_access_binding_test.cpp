#include "prepared_access_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::prepared_access;

TEST(PreparedAccessBindingTest, SequentialAnalysesUseEachElfBaseAndBasis)
{
  const auto raw = kernel({loop(3, {access("global::A", "i")})});
  for (const auto base : {0x1000U, 0x9000U})
  {
    auto model = addresses();
    model.objects["global::A"].base = base;
    model.basis =
      base == 0x1000 ? AddressBasis::Absolute : AddressBasis::ImageRelative;
    const auto result = resolved_task_traces(raw, model);
    ASSERT_EQ(result.tasks.size(), 1U);
    ASSERT_EQ(result.tasks[0].accesses.size(), 3U);
    for (std::size_t i = 0; i < 3; ++i)
      expect_access(result.tasks[0].accesses[i],
                    {"global::A", 4 * i, 4, base + 4 * i, model.basis,
                     AccessOperation::Load, i});
  }
}

TEST(PreparedAccessBindingTest, ChangedElfExtentIsRecheckedAndFailureIsIsolated)
{
  const auto raw = kernel({loop(3, {access("global::A", "i")})});
  const auto large = addresses();
  auto small = large;
  small.objects["global::A"].size = 8;
  EXPECT_EQ(resolved_task_traces(raw, large).coverage.resolved_accesses, 3U);
  expect_resolution_failure(raw, small, "access exceeds ELF object extent", 2);
  EXPECT_EQ(resolved_task_traces(raw, large).coverage.resolved_accesses, 3U);
}

TEST(PreparedAccessBindingTest, SequentialLayoutsKeepEachAbiPaddingAndStride)
{
  auto raw = kernel({loop(3, {structured_access()}, "j")}, structured_module());
  auto & node = raw["functions"][0]["body"][0]["body"][0];
  node["indices"][0] = "0";
  node["access_path"][0]["value"] = "0";
  for (const auto cell_size : {16, 24})
  {
    raw["metadata"]["objects"]["global::A"]["elem_size"] = cell_size;
    raw["metadata"]["structs"]["Cell"]["size"] = cell_size;
    raw["metadata"]["structs"]["Cell"]["fields"][1]["offset"] = cell_size - 8;
    const auto result = resolved_task_traces(raw, addresses());
    ASSERT_EQ(result.tasks[0].accesses.size(), 3U);
    for (std::size_t i = 0; i < 3; ++i)
    {
      const auto offset = cell_size * (i + 1) - 8;
      expect_access(result.tasks[0].accesses[i],
                    {"global::A", offset, 8, 0x101c + offset,
                     AddressBasis::Absolute, AccessOperation::Load, i});
    }
  }
}

TEST(PreparedAccessBindingTest, ReentrantStructuredAnalysisKeepsOuterPlan)
{
  const auto raw = kernel({loop(2, {loop(2, {structured_access()}, "j")})},
                          structured_module());
  auto other = addresses();
  other.objects["global::A"].base = 0x8000;
  Collector collector;
  auto sink = collector.sink();
  const auto record = sink.access;
  sink.access = [&](const std::string & id, const ResolvedAccess & value) {
    record(id, value);
    if (value.source_access_ordinal != 1) return;
    const auto inner = resolved_task_traces(raw, other);
    ASSERT_EQ(inner.tasks[0].accesses.size(), 4U);
    EXPECT_EQ(inner.tasks[0].accesses.back().linked_byte_address, 0x8048U);
  };
  collector.complete(stream_resolved_task_accesses(raw, addresses(), sink));
  const std::vector<std::uint64_t> offsets{8, 24, 56, 72};
  ASSERT_EQ(collector.result.tasks[0].accesses.size(), offsets.size());
  for (std::size_t i = 0; i < offsets.size(); ++i)
    expect_access(collector.result.tasks[0].accesses[i],
                  {"global::A", offsets[i], 8, 0x101c + offsets[i],
                   AddressBasis::Absolute, AccessOperation::Load, i});
}

}  // namespace
