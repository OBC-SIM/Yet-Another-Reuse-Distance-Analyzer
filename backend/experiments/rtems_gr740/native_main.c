#include "workload.h"

/** @brief Check the numerical workload on the host. @return Zero on success. */
int main(void)
{
  for (int run = 0; run < 2; ++run)
  {
    benchmark_initialize();
    benchmark_kernel();
    if (!benchmark_validate()) return 1;
  }
  return 0;
}
