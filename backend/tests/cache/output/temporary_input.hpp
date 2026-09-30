#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace yarda::test::artifact
{

/** @brief Own a unique temporary directory for one independent file test. */
class TemporaryInput
{
public:
  TemporaryInput()
  {
    auto name =
        (std::filesystem::temp_directory_path() / "yarda-artifact-XXXXXX")
            .string();
    std::vector<char> writable(name.begin(), name.end());
    writable.push_back('\0');
    const auto * created = mkdtemp(writable.data());
    if (!created) throw std::runtime_error("cannot create artifact fixture");
    directory = created;
    path = (directory / "input").string();
  }

  ~TemporaryInput()
  {
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
  }

  TemporaryInput(const TemporaryInput &) = delete;
  TemporaryInput & operator=(const TemporaryInput &) = delete;

  void write(const std::string & bytes) const
  {
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
  }

  std::filesystem::path directory;
  std::string path;
};

} // namespace yarda::test::artifact
