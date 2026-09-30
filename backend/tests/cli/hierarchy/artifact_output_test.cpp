#include "test_support.hpp"
#include "cli/artifact_output.hpp"

namespace
{
using namespace yarda::test::cli;
using yarda::cli::publish_json_artifacts;

TEST(ArtifactOutputTest, ReplacesExistingFilesAndAddsExactlyOneLf)
{
  TemporaryInput tmp;
  const auto second = (tmp.directory / "second").string();
  write(tmp.path, "original");
  publish_json_artifacts({}, {{tmp.path, "{}"}, {second, "[]"}});
  EXPECT_EQ(read(tmp.path), "{}\n");
  EXPECT_EQ(read(second), "[]\n");
  EXPECT_EQ(std::distance(fs::directory_iterator(tmp.directory),
                          fs::directory_iterator{}),
            2);
}

TEST(ArtifactOutputTest, StagingFailurePreservesAllExistingFiles)
{
  TemporaryInput tmp;
  write(tmp.path, "original");
  const auto missing = (tmp.directory / "missing" / "result").string();
  EXPECT_THROW(publish_json_artifacts({}, {{tmp.path, "{}"}, {missing, "[]"}}),
               std::runtime_error);
  EXPECT_EQ(read(tmp.path), "original");
  EXPECT_EQ(std::distance(fs::directory_iterator(tmp.directory),
                          fs::directory_iterator{}),
            1);
}

TEST(ArtifactOutputTest, LaterRenameFailureRestoresOldFilesAndRemovesNewOutputs)
{
  TemporaryInput tmp;
  write(tmp.path, "original");
  const auto second = (tmp.directory / "new").string();
  const auto last = (tmp.directory / "last").string();
  write(last, "last original");
  unsigned attempts = 0;
  const auto fail_last = [&](const fs::path & from, const fs::path & to)
  {
    if (++attempts == 3) throw std::runtime_error("injected rename failure");
    fs::rename(from, to);
  };
  EXPECT_THROW(
      publish_json_artifacts(
          {}, {{tmp.path, "{}"}, {second, "[]"}, {last, "42"}}, fail_last),
      std::runtime_error);
  EXPECT_EQ(attempts, 3U);
  EXPECT_EQ(read(tmp.path), "original");
  EXPECT_EQ(read(last), "last original");
  EXPECT_FALSE(fs::exists(second));
  EXPECT_EQ(std::distance(fs::directory_iterator(tmp.directory),
                          fs::directory_iterator{}),
            2);
}

TEST(ArtifactOutputTest, RejectsOutputInputHardLinkAliasesWithoutChangingInput)
{
  TemporaryInput tmp;
  write(tmp.path, "input bytes");
  const auto alias = (tmp.directory / "alias").string();
  fs::create_hard_link(tmp.path, alias);
  EXPECT_THROW(publish_json_artifacts({tmp.path}, {{alias, "{}"}}),
               std::invalid_argument);
  EXPECT_EQ(read(tmp.path), "input bytes");
}

TEST(ArtifactOutputTest, RejectsDuplicateNormalizedOutputPaths)
{
  TemporaryInput tmp;
  const auto alias = (tmp.directory / "." / "input").string();
  EXPECT_THROW(publish_json_artifacts({}, {{tmp.path, "{}"}, {alias, "[]"}}),
               std::invalid_argument);
  EXPECT_FALSE(fs::exists(tmp.path));
}

TEST(ArtifactOutputTest, RejectsOutputHardLinkAliases)
{
  TemporaryInput tmp;
  write(tmp.path, "old");
  const auto alias = (tmp.directory / "alias").string();
  fs::create_hard_link(tmp.path, alias);
  EXPECT_THROW(publish_json_artifacts({}, {{tmp.path, "{}"}, {alias, "[]"}}),
               std::invalid_argument);
  EXPECT_EQ(read(alias), "old");
}

TEST(ArtifactOutputTest, RejectsSymlinksAndDirectories)
{
  TemporaryInput tmp;
  write(tmp.path, "old");
  const auto link = (tmp.directory / "link").string();
  fs::create_symlink(tmp.path, link);
  EXPECT_THROW(publish_json_artifacts({}, {{link, "{}"}}),
               std::invalid_argument);
  EXPECT_THROW(publish_json_artifacts({}, {{tmp.directory.string(), "{}"}}),
               std::invalid_argument);
  EXPECT_EQ(read(tmp.path), "old");
}

TEST(ArtifactOutputTest, RejectsStdoutAndEmptyPaths)
{
  EXPECT_THROW(publish_json_artifacts({}, {{"-", "{}"}}),
               std::invalid_argument);
  EXPECT_THROW(publish_json_artifacts({}, {{"", "{}"}}), std::invalid_argument);
}
} // namespace
