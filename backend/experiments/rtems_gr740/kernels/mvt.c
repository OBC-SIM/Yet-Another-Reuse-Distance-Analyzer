/* Adapted from PolyBench/C 4.2.1; see ../POLYBENCH-LICENSE.txt. */
#include "../workload.h"

static double A[N][N] __attribute__((aligned(32)));
static double x1[N] __attribute__((aligned(32)));
static double x2[N] __attribute__((aligned(32)));
static double y1[N] __attribute__((aligned(32)));
static double y2[N] __attribute__((aligned(32)));

void benchmark_initialize(void)
{
  for (int i = 0; i < N; ++i)
  {
    x1[i] = x2[i] = 0;
    y1[i] = y2[i] = 1;
    for (int j = 0; j < N; ++j) A[i][j] = i + 2 * j + 1;
  }
}

void benchmark_kernel(void)
{
#pragma APE_ANALYZE_BEGIN
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j) x1[i] = x1[i] + A[i][j] * y1[j];
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j) x2[i] = x2[i] + A[j][i] * y2[j];
#pragma APE_ANALYZE_END
}

int benchmark_validate(void)
{
  for (int i = 0; i < N; ++i)
    if (x1[i] != (double)N * (i + N) ||
        x2[i] != (double)N * (2 * i + 1) + (double)N * (N - 1) / 2) return 0;
  return 1;
}
