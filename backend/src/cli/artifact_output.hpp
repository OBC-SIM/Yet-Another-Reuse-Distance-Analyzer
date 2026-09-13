#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace yarda::cli
{

/** @brief Own a complete serialized artifact, without the final LF. */
struct JsonArtifact
{
  std::string path;
  std::string document;
};

/**
 * @brief Rename a staged file or throw before changing either path.
 *
 * An empty callback selects filesystem rename. Injection permits deterministic
 * publication-failure tests; rollback always uses the real filesystem.
 */
using ArtifactRename = std::function<void(const std::filesystem::path &,
                                          const std::filesystem::path &)>;

/**
 * @brief Stage all documents, then publish in order with handled-error
 * rollback.
 * @param inputs Borrowed input paths which must never be overwritten.
 * @param artifacts Borrowed complete documents and distinct regular-file paths.
 * Existing files are backed up before replacement. Symlinks, special files,
 * stdout, input aliases and duplicate output aliases are rejected.
 * @param rename Optional publication operation, following ArtifactRename's
 * contract.
 * @return Nothing; each document is published with exactly one final LF.
 * @throws std::exception for invalid paths or I/O failure. Handled publication
 * failures restore prior files and remove new outputs. A failed restoration
 * reports its retained backup path. Callers must own the paths exclusively;
 * process crashes and concurrent writers are outside this rollback contract.
 * @note Cleanup failure after publication reports the complete published set
 * and the retained staging path; it does not attempt rollback after commit.
 */
void publish_json_artifacts(const std::vector<std::string> & inputs,
                            const std::vector<JsonArtifact> & artifacts,
                            const ArtifactRename & rename = {});

} // namespace yarda::cli
