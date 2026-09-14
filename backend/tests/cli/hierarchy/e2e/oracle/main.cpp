#include "fixture.hpp"

#include <iostream>
#include "llvm/Support/ManagedStatic.h"

std::array<std::string, 6> yarda::test::e2e::input_paths;

/**
 * @brief Verify one completed CLI run against source expectations and oracles.
 * @param argc Argument count: program and six input paths, plus GTest options.
 * @param argv Borrowed non-null arguments: LAT, ELF, YAML, RESULT, EVENTS, golden.
 * @return Google Test status, or 2 for invalid arguments.
 */
int main(int argc, char ** argv)
{
  llvm::llvm_shutdown_obj shutdown;
  testing::InitGoogleTest(&argc, argv);
  if (argc != 7)
  {
    std::cerr << "expected LAT ELF CACHE RESULT EVENTS GOLDEN\n";
    return 2;
  }
  for (std::size_t i = 0; i < 6; ++i)
    yarda::test::e2e::input_paths[i] = argv[i + 1];
  return RUN_ALL_TESTS();
}
