#include "../workload.h"

static double lines[DOMAIN][4] __attribute__((aligned(32)));

void benchmark_initialize(void)
{
  for (int i = 0; i < DOMAIN; ++i) lines[i][0] = 0;
}

void benchmark_kernel(void)
{
#pragma APE_ANALYZE_BEGIN
  for (int repeat = 0; repeat < REPEATS; ++repeat)
    for (int i = 0; i < DOMAIN; ++i) lines[i][0] += 1;
#pragma APE_ANALYZE_END
}

int benchmark_validate(void)
{
  for (int i = 0; i < DOMAIN; ++i) if (lines[i][0] != REPEATS) return 0;
  return 1;
}
