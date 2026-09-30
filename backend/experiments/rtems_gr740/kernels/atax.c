/* Adapted from PolyBench/C 4.2.1; see ../POLYBENCH-LICENSE.txt.
 * Static, aligned storage and literal bounds replace PolyBench allocation.
 */
#include "../workload.h"

static double A[M][N] __attribute__((aligned(32)));
static double x[N] __attribute__((aligned(32)));
static double y[N] __attribute__((aligned(32)));
static double tmp[M] __attribute__((aligned(32)));

void benchmark_initialize(void)
{
  for (int j = 0; j < N; ++j) x[j] = 1;
  for (int i = 0; i < M; ++i)
    for (int j = 0; j < N; ++j) A[i][j] = i + 2 * j + 1;
}

void benchmark_kernel(void)
{
#pragma APE_ANALYZE_BEGIN
  for (int j = 0; j < N; ++j) y[j] = 0;
  for (int i = 0; i < M; ++i)
  {
    tmp[i] = 0;
    for (int j = 0; j < N; ++j) tmp[i] = tmp[i] + A[i][j] * x[j];
    for (int j = 0; j < N; ++j) y[j] = y[j] + A[i][j] * tmp[i];
  }
#pragma APE_ANALYZE_END
}

int benchmark_validate(void)
{
  const double sum_i = (double)M * (M - 1) / 2;
  const double sum_i2 = (double)M * (M - 1) * (2 * M - 1) / 6;
  for (int i = 0; i < M; ++i) if (tmp[i] != (double)N * (i + N)) return 0;
  for (int j = 0; j < N; ++j)
  {
    const double expected = N * (sum_i2 + (N + 2 * j + 1) * sum_i + (double)M * N * (2 * j + 1));
    if (y[j] != expected) return 0;
  }
  return 1;
}
