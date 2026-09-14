#include <gtest/gtest.h>

#include "driver/evaluation.hpp"
#include "cache/streaming/streaming_hierarchy_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::evaluation;
using namespace yarda::test::streaming;

TEST(EvaluationReportTest, ReportsMedianExtremaAndInterquartileRange)
{
  const auto stats = distribution({9, 1, 7, 3, 5});
  EXPECT_EQ(stats.at("median"), 5);
  EXPECT_EQ(stats.at("min"), 1);
  EXPECT_EQ(stats.at("max"), 9);
  EXPECT_EQ(stats.at("iqr"), 4);
}

TEST(EvaluationReportTest, AveragesEvenMedianWithoutIntegerOverflow)
{
  const auto value = std::numeric_limits<std::uint64_t>::max();
  const auto stats = distribution({value, value});
  EXPECT_EQ(stats.at("median").get<double>(), static_cast<double>(value));
}

TEST(EvaluationReportTest, RejectsAnEmptySuccessfulSampleSet)
{
  EXPECT_THROW(distribution({}), std::invalid_argument);
}

TEST(EvaluationBudgetTest, AcceptsInclusiveSourceAndLineLimits)
{
  const auto raw = byte_trace({0, 32, 0});
  StreamingHierarchyOptions options;
  options.emission_limits = {3, 3};
  const auto batch = run_batch(raw, byte_addresses(), make_batch_hierarchy(),
                                options);
  const auto stream = analyze_streaming_hierarchy(raw, byte_addresses(),
                                                   make_batch_hierarchy(), options);
  ASSERT_EQ(batch.tasks.size(), 1U);
  expect_summary(batch.tasks[0].summary, stream.tasks[0]);
}

TEST(EvaluationBudgetTest, RejectsSourceExhaustionAcrossTaskBoundaries)
{
  const auto raw = byte_module(Json::array({function("first", byte_body({0})),
                                           function("next", byte_body({32}))}));
  StreamingHierarchyOptions options;
  options.emission_limits = {1, 100};
  EXPECT_THROW(run_batch(raw, byte_addresses(), make_batch_hierarchy(), options),
               std::invalid_argument);
}

TEST(EvaluationBudgetTest, ChargesEveryCrossLineSpan)
{
  const auto raw = stream::module(Json::array({function("crossing",
    Json::array({access()}))}), 8);
  StreamingHierarchyOptions options;
  options.emission_limits = {100, 1};
  EXPECT_THROW(run_batch(raw, stream::addresses(8), make_batch_hierarchy(), options),
               std::invalid_argument);
  options.emission_limits.emitted_line_references = 2;
  EXPECT_EQ(run_batch(raw, stream::addresses(8), make_batch_hierarchy(), options)
              .coverage.emitted_line_references, 2U);
}

TEST(EvaluationBudgetTest, RaisedLoopAllowanceAppliesToBatchCollection)
{
  const auto raw = byte_module(Json::array({function("empty",
    Json::array({loop(1000001, Json::array())}))}));
  StreamingHierarchyOptions options;
  EXPECT_THROW(run_batch(raw, byte_addresses(), make_batch_hierarchy(), options),
               std::invalid_argument);
  options.loop_limits = {1000001, 1000001};
  const auto result = run_batch(raw, byte_addresses(), make_batch_hierarchy(),
                                 options);
  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.coverage.source_accesses, 0U);
}

TEST(EvaluationFailureTest, ClassifiesOnlyKnownBudgetMessagesAsExhaustion)
{
  EXPECT_EQ(failure_status(std::invalid_argument("emitted line references exceeds 0")),
            "budget_exhaustion");
  EXPECT_EQ(failure_status(std::runtime_error("unexpected failure")), "error");
  EXPECT_EQ(failure_status(std::bad_alloc()), "resource_failure");
}
} // namespace
