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
} // namespace
