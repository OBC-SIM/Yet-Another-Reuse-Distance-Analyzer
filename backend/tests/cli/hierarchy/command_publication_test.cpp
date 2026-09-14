#include "test_support.hpp"

#include <set>
#include <vector>

namespace
{
using namespace yarda::test::cli;
using yarda::cli::ArtifactRename;
using yarda::cli::run_hierarchy_command;

class HierarchyPublicationTest : public HierarchyCommandTest
{
protected:
  std::string publication_error(const ArtifactRename & rename)
  {
    try
    {
      run_hierarchy_command(options, nullptr, rename);
      ADD_FAILURE() << "publication unexpectedly succeeded";
    }
    catch (const std::runtime_error & error)
    {
      return error.what();
    }
    return {};
  }

  void expect_no_staging() const
  {
    for (const auto & entry : fs::directory_iterator(temporary.directory))
      EXPECT_EQ(entry.path().filename().string().find(".yarda-json-"),
                std::string::npos);
  }
};

class HierarchyPublicationOptionsTest
    : public HierarchyPublicationTest,
      public ::testing::WithParamInterface<unsigned>
{};

TEST_P(HierarchyPublicationOptionsTest, PublishesResultAfterRequestedDiagnostics)
{
  diagnostics();
  if (!(GetParam() & 1U)) options.events_path.clear();
  if (!(GetParam() & 2U)) options.telemetry_path.clear();
  const std::string previous_result = "previous result\n";
  write(options.export_path, previous_result);
  std::set<fs::path> diagnostic_paths;
  if (!options.events_path.empty()) diagnostic_paths.insert(options.events_path);
  if (!options.telemetry_path.empty())
    diagnostic_paths.insert(options.telemetry_path);

  std::vector<fs::path> published;
  const auto observe = [&](const fs::path & from, const fs::path & to)
  {
    if (to == options.export_path)
    {
      EXPECT_EQ(published.size(), diagnostic_paths.size());
      EXPECT_EQ(std::set<fs::path>(published.begin(), published.end()),
                diagnostic_paths);
    }
    else
    {
      EXPECT_EQ(diagnostic_paths.count(to), 1U);
      EXPECT_EQ(read(options.export_path), previous_result);
    }
    fs::rename(from, to);
    published.push_back(to);
  };
  run_hierarchy_command(options, nullptr, observe);
  ASSERT_EQ(published.size(), diagnostic_paths.size() + 1);
  EXPECT_EQ(published.back(), fs::path(options.export_path));
  EXPECT_NE(read(options.export_path), previous_result);
  expect_no_staging();
}

INSTANTIATE_TEST_SUITE_P(DiagnosticOptions, HierarchyPublicationOptionsTest,
                        ::testing::Values(0U, 1U, 2U, 3U));

TEST_F(HierarchyPublicationTest, FirstDiagnosticRenameFailurePublishesNothing)
{
  diagnostics();
  unsigned attempts = 0;
  fs::path failed_path;
  const auto fail = [&](const fs::path &, const fs::path & to)
  {
    ++attempts;
    failed_path = to;
    EXPECT_NE(to, fs::path(options.export_path));
    EXPECT_FALSE(fs::exists(options.export_path));
    EXPECT_FALSE(fs::exists(options.events_path));
    EXPECT_FALSE(fs::exists(options.telemetry_path));
    throw std::runtime_error("injected first rename failure");
  };
  const auto error = publication_error(fail);
  EXPECT_EQ(attempts, 1U);
  EXPECT_NE(error.find("injected first rename failure"), std::string::npos);
  EXPECT_NE(error.find(failed_path.string()), std::string::npos);
  expect_no_outputs();
}

TEST_F(HierarchyPublicationTest,
       LaterDiagnosticRenameFailureRestoresFilesWithoutPublishingResult)
{
  diagnostics();
  const std::string previous_result = "previous result\n";
  const std::string previous_diagnostic = "previous diagnostic\n";
  write(options.export_path, previous_result);
  write(options.events_path, previous_diagnostic);
  write(options.telemetry_path, previous_diagnostic);
  unsigned attempts = 0;
  fs::path published_path;
  fs::path failed_path;
  const auto fail_later = [&](const fs::path & from, const fs::path & to)
  {
    ++attempts;
    EXPECT_NE(to, fs::path(options.export_path));
    EXPECT_EQ(read(options.export_path), previous_result);
    if (!published_path.empty())
    {
      failed_path = to;
      EXPECT_NE(read(published_path), previous_diagnostic);
      throw std::runtime_error("injected later diagnostic rename failure");
    }
    fs::rename(from, to);
    published_path = to;
  };
  const auto error = publication_error(fail_later);
  EXPECT_EQ(attempts, 2U);
  EXPECT_NE(error.find("injected later diagnostic rename failure"),
            std::string::npos);
  EXPECT_NE(error.find(failed_path.string()), std::string::npos);
  EXPECT_EQ(read(options.export_path), previous_result);
  EXPECT_EQ(read(options.events_path), previous_diagnostic);
  EXPECT_EQ(read(options.telemetry_path), previous_diagnostic);
  expect_no_staging();
}

TEST_F(HierarchyPublicationTest,
       ResultRenameFailureRestoresExistingAndRemovesNewDiagnostics)
{
  diagnostics();
  const std::string previous_result = "previous result\n";
  const std::string previous_events = "previous events\n";
  write(options.export_path, previous_result);
  write(options.events_path, previous_events);
  unsigned attempts = 0;
  std::set<fs::path> published;
  const std::set<fs::path> diagnostics{options.events_path,
                                      options.telemetry_path};
  const auto fail_result = [&](const fs::path & from, const fs::path & to)
  {
    ++attempts;
    EXPECT_EQ(read(options.export_path), previous_result);
    if (to == options.export_path)
    {
      EXPECT_EQ(published, diagnostics);
      EXPECT_NE(read(options.events_path), previous_events);
      EXPECT_TRUE(fs::exists(options.telemetry_path));
      throw std::runtime_error("injected RESULT rename failure");
    }
    fs::rename(from, to);
    published.insert(to);
  };
  const auto error = publication_error(fail_result);
  EXPECT_EQ(attempts, 3U);
  EXPECT_NE(error.find("injected RESULT rename failure"), std::string::npos);
  EXPECT_NE(error.find(options.export_path), std::string::npos);
  EXPECT_EQ(read(options.export_path), previous_result);
  EXPECT_EQ(read(options.events_path), previous_events);
  EXPECT_FALSE(fs::exists(options.telemetry_path));
  expect_no_staging();
}
} // namespace
