#include "artifact_output.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace yarda::cli
{
namespace
{

namespace fs = std::filesystem;

struct StagedArtifact
{
  fs::path target;
  fs::path directory;
  bool has_previous = false;
  bool published = false;
  bool preserve_backup = false;
};

bool aliases(const fs::path & left, const fs::path & right)
{
  return fs::weakly_canonical(left) == fs::weakly_canonical(right) ||
         (fs::exists(left) && fs::exists(right) && fs::equivalent(left, right));
}

void validate_paths(const std::vector<std::string> & inputs,
                    const std::vector<JsonArtifact> & artifacts)
{
  for (std::size_t index = 0; index < artifacts.size(); ++index)
  {
    const auto & path = artifacts[index].path;
    if (path.empty() || path == "-" || path.find('\0') != std::string::npos)
      throw std::invalid_argument("invalid artifact export path: " + path);
    const auto status = fs::symlink_status(path);
    if (fs::exists(status) && !fs::is_regular_file(status))
      throw std::invalid_argument(
          "artifact export path must be a regular file: " + path);
    for (const auto & input : inputs)
      if (aliases(path, input))
        throw std::invalid_argument("artifact output aliases input: " + path);
    for (std::size_t earlier = 0; earlier < index; ++earlier)
      if (aliases(path, artifacts[earlier].path))
        throw std::invalid_argument("duplicate artifact output alias: " + path);
  }
}

fs::path staging_directory(const fs::path & target)
{
  const auto pattern = (target.parent_path() / ".yarda-json-XXXXXX").string();
  std::vector<char> writable(pattern.begin(), pattern.end());
  writable.push_back('\0');
  const auto * created = mkdtemp(writable.data());
  if (!created) throw std::runtime_error(std::strerror(errno));
  return created;
}

std::string clean_staging(const std::vector<StagedArtifact> & staged)
{
  std::string errors;
  for (const auto & item : staged)
  {
    if (item.directory.empty() || item.preserve_backup) continue;
    std::error_code error;
    fs::remove_all(item.directory, error);
    if (error)
      errors += "; cannot remove staging " + item.directory.string() + ": " +
                error.message();
  }
  return errors;
}

std::string rollback(std::vector<StagedArtifact> & staged)
{
  std::string errors;
  for (auto item = staged.rbegin(); item != staged.rend(); ++item)
  {
    if (!item->published) continue;
    std::error_code error;
    if (item->has_previous)
      fs::rename(item->directory / "previous", item->target, error);
    else
      fs::remove(item->target, error);
    if (error)
    {
      item->preserve_backup = true;
      errors += "; cannot restore " + item->target.string() + ": " +
                error.message() +
                "; retained staging/backup: " + item->directory.string();
    }
  }
  return errors;
}

} // namespace

void publish_json_artifacts(const std::vector<std::string> & inputs,
                            const std::vector<JsonArtifact> & artifacts,
                            const ArtifactRename & rename)
{
  validate_paths(inputs, artifacts);
  std::vector<StagedArtifact> staged;
  staged.reserve(artifacts.size());
  std::string current_path;
  try
  {
    for (const auto & artifact : artifacts)
    {
      current_path = artifact.path;
      auto & item = staged.emplace_back();
      item.target = fs::absolute(artifact.path);
      item.directory = staging_directory(item.target);
      std::ofstream output;
      output.exceptions(std::ios::failbit | std::ios::badbit);
      output.open(item.directory / "document", std::ios::binary);
      output << artifact.document << '\n';
      output.close();
      if (fs::exists(item.target))
      {
        fs::create_hard_link(item.target, item.directory / "previous");
        item.has_previous = true;
      }
    }
    for (auto & item : staged)
    {
      current_path = item.target.string();
      if (rename)
        rename(item.directory / "document", item.target);
      else
        fs::rename(item.directory / "document", item.target);
      item.published = true;
    }
  }
  catch (const std::exception & error)
  {
    const auto restoration_errors = rollback(staged);
    const auto cleanup_errors = clean_staging(staged);
    throw std::runtime_error("failed artifact export path: " + current_path +
                             ": " + error.what() + restoration_errors +
                             cleanup_errors);
  }
  catch (...)
  {
    const auto restoration_errors = rollback(staged);
    const auto cleanup_errors = clean_staging(staged);
    throw std::runtime_error("failed artifact export path: " + current_path +
                             restoration_errors + cleanup_errors);
  }
  const auto cleanup_errors = clean_staging(staged);
  if (!cleanup_errors.empty())
    throw std::runtime_error("artifacts published but staging cleanup failed" +
                             cleanup_errors);
}

} // namespace yarda::cli
