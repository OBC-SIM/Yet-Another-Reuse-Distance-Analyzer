#include "test_support.hpp"

namespace
{
using namespace yarda::test::cli;

TEST(HierarchyOptionsTest, RejectsEmptyOrWhitespaceNumbers)
{
  for (const std::string value : {"", " ", " 1", "1 ", "0x10"})
  {
    SCOPED_TRACE(value);
    EXPECT_THROW(
        parse({"in", "--analysis", "hierarchy-rd", "--elf", "elf", "--cache",
               "cache", "--export", "out", "--max-source-accesses", value}),
        std::invalid_argument);
  }
}

TEST(HierarchyOptionsTest, RejectsStdoutOrEmptyArtifactValues)
{
  for (const auto * flag : {"--export", "--export-events", "--telemetry"})
    for (const auto * value : {"", "-"})
    {
      SCOPED_TRACE(flag);
      EXPECT_THROW(parse({"in", "--analysis", "hierarchy-rd", "--elf", "elf",
                          "--cache", "cache", "--export", "out", flag, value}),
                   std::invalid_argument);
    }
}

TEST(HierarchyOptionsTest, PreservesLegacyDuplicateLastValueAndGranularity)
{
  const auto options = parse({"in", "--mode", "unroll", "--cache", "old",
                              "--cache", "new", "--granularity", "element"});
  EXPECT_EQ(options.cache_path, "new");
  EXPECT_EQ(options.analysis_mode, yarda::cli::AnalysisMode::Legacy);
  EXPECT_EQ(options.granularity, yarda::Granularity::Element);
}
TEST(HierarchyOptionsTest, CombinesIrCountingWithHierarchyInEitherOrder)
{
  for (const bool ir_first : {false, true})
  {
    const auto options = parse({
      "in", "--analysis", ir_first ? "ir-instructions" : "hierarchy-rd",
      "--analysis", ir_first ? "hierarchy-rd" : "ir-instructions",
      "--elf", "elf", "--cache", "cache", "--export", "out"});
    EXPECT_EQ(options.analysis_mode, yarda::cli::AnalysisMode::HierarchyRd);
    EXPECT_TRUE(options.ir_instructions);
  }
}

TEST(HierarchyOptionsTest, RejectsConflictingMemoryAnalyses)
{
  EXPECT_THROW(parse({"in", "--analysis", "mapping", "--analysis",
                      "hierarchy-rd", "--elf", "elf", "--cache", "cache",
                      "--export", "out"}), std::invalid_argument);
}

TEST(HierarchyOptionsTest, DuplicateIrAnalysisDoesNotDoubleCount)
{
  const auto options = parse({"in", "--analysis", "ir-instructions",
                              "--analysis", "ir-instructions"});
  EXPECT_EQ(options.analysis_mode, yarda::cli::AnalysisMode::IrInstructions);
  EXPECT_TRUE(options.ir_instructions);
}
} // namespace
