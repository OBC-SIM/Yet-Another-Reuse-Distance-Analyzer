#include "test_support.hpp"
#include "cli/artifact_output.hpp"

#include <csignal>
#include <sys/resource.h>
#include <unistd.h>

namespace
{
using namespace yarda::test::cli;

TEST(ArtifactOutputTest, ActualWriteFailurePreservesOldFilesAndCleansStaging)
{
  TemporaryInput tmp;
  write(tmp.path, "previous bytes");
  const auto large = (tmp.directory / "large").string();
  const auto write_limited = [&]
  {
    // Restrict only the child; real stream writes fail without filling a disk.
    std::signal(SIGXFSZ, SIG_IGN);
    const rlimit limit{16, 16};
    if (setrlimit(RLIMIT_FSIZE, &limit) != 0) _exit(2);
    try
    {
      yarda::cli::publish_json_artifacts(
          {}, {{tmp.path, "{}"}, {large, std::string(1024, 'a')}});
    }
    catch (const std::runtime_error & error)
    {
      _exit(std::string(error.what()).find(large) != std::string::npos ? 0 : 4);
    }
    _exit(3);
  };
  EXPECT_EXIT(write_limited(), ::testing::ExitedWithCode(0), "");
  EXPECT_EQ(read(tmp.path), "previous bytes");
  EXPECT_FALSE(fs::exists(large));
  EXPECT_EQ(std::distance(fs::directory_iterator(tmp.directory),
                          fs::directory_iterator{}),
            1);
}
} // namespace
