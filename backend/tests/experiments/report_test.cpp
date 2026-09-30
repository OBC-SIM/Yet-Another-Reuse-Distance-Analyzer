#include <gtest/gtest.h>

#include "driver/evaluation.hpp"
#include "cache/output/temporary_input.hpp"

namespace
{
using namespace yarda::evaluation;
using yarda::test::artifact::TemporaryInput;

nlohmann::json sample(std::uint64_t time)
{
  return {{"case_id", "fixture"}, {"mode", "streaming"}, {"status", "success"},
          {"analysis_id", "same-inputs"}, {"tool_version", "same-binary"},
          {"binary_sha256", "same-binary-bytes"},
          {"source_accesses", 8}, {"line_references", 8},
          {"tasks", nlohmann::json::array()},
          {"total_time_ns", time}, {"analysis_time_ns", time},
          {"input_time_ns", 0}, {"serialize_time_ns", 0}, {"peak_rss_bytes", 4096}};
}

TEST(EvaluationReportTest, PreservesSmallDispersionNearUint64Maximum)
{
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  EXPECT_EQ(distribution({maximum - 4, maximum - 2, maximum}).at("iqr"), 2);
}

TEST(EvaluationReportTest, ExcludesFailuresFromSuccessfulDistributions)
{
  TemporaryInput first, second, failed;
  first.write(sample(10).dump());
  second.write(sample(30).dump());
  failed.write(nlohmann::json{{"case_id", "fixture"}, {"mode", "streaming"},
                             {"status", "timeout"}}.dump());
  const auto report = summarize_samples({first.path, second.path, failed.path});
  ASSERT_EQ(report.size(), 1U);
  EXPECT_EQ(report[0].at("total_time_ns").at("median"), 20);
  EXPECT_EQ(report[0].at("total_time_ns").at("count"), 2);
  EXPECT_EQ(report[0].at("statuses").at("timeout"), 1);
  EXPECT_EQ(report[0].at("analysis_id"), "same-inputs");
}

TEST(EvaluationReportTest, RejectsMixingInputIdentitiesInOneGroup)
{
  TemporaryInput first, second;
  first.write(sample(10).dump());
  auto changed = sample(30);
  changed["analysis_id"] = "different-inputs";
  second.write(changed.dump());
  EXPECT_THROW(summarize_samples({first.path, second.path}), std::invalid_argument);
}

TEST(EvaluationReportTest, KeepsFailedOnlyGroupsWithoutInventingTimings)
{
  TemporaryInput input;
  input.write(nlohmann::json{{"case_id", "fixture"}, {"mode", "batch"},
                             {"status", "resource_failure"}}.dump());
  const auto report = summarize_samples({input.path});
  EXPECT_FALSE(report[0].contains("total_time_ns"));
  EXPECT_EQ(report[0].at("statuses").at("resource_failure"), 1);
}

TEST(EvaluationReportTest, RejectsDifferentBinariesWithTheSameToolVersion)
{
  TemporaryInput first, second;
  first.write(sample(10).dump());
  auto changed = sample(30);
  changed["binary_sha256"] = "different-binary-bytes";
  second.write(changed.dump());
  EXPECT_THROW(summarize_samples({first.path, second.path}), std::invalid_argument);
}

TEST(EvaluationInputTest, RejectsNegativeFractionalAndStringWorkLimits)
{
  nlohmann::json limits{{"single_loop", 0}, {"cumulative_loop", 0},
                        {"source_accesses", 0}, {"line_references", 0}};
  EXPECT_EQ(work_options({{"effective_work_limits", limits}})
              .emission_limits.emitted_source_accesses, 0U);
  for (const nlohmann::json & bad : {nlohmann::json(-1), nlohmann::json(1.5),
                                  nlohmann::json("10"), nlohmann::json(true)})
  {
    auto changed = limits;
    changed["source_accesses"] = bad;
    EXPECT_THROW(work_options({{"effective_work_limits", changed}}),
                 std::invalid_argument);
  }
}
} // namespace
