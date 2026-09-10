#include <gtest/gtest.h>

#include "llvm/Support/ManagedStatic.h"

/**
 * @brief Run linked-region tests and release LLVM's managed process state.
 * @param argc Number of test runner arguments.
 * @param argv Borrowed non-null argument array.
 * @return Google Test's exit status.
 */
int main(int argc, char ** argv)
{
  llvm::llvm_shutdown_obj shutdown;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
