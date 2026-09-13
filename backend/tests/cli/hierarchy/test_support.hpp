#pragma once

#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>
#include "cache/output/temporary_input.hpp"

namespace yarda::test::cli
{

namespace fs = std::filesystem;
using artifact::TemporaryInput;

inline void write(const fs::path & path, const std::string & text)
{
  std::ofstream file;
  file.exceptions(std::ios::failbit | std::ios::badbit);
  file.open(path, std::ios::binary);
  file << text;
  file.close();
}

inline std::string read(const fs::path & path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("missing test artifact: " + path.string());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

} // namespace yarda::test::cli
