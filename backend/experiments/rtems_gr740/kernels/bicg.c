/* Adapted from PolyBench/C 4.2.1; see ../POLYBENCH-LICENSE.txt. */
#include "../workload.h"

static double A[N][M] __attribute__((aligned(32)));
static double s[M] __attribute__((aligned(32)));
static double q[N] __attribute__((aligned(32)));
static double p[M] __attribute__((aligned(32)));
static double r[N] __attribute__((aligned(32)));

void benchmark_initialize(void)
{
  for (int j = 0; j < M; ++j) p[j] = 1;
  for (int i = 0; i < N; ++i)
  {
    r[i] = 1;
    for (int j = 0; j < M; ++j) A[i][j] = i + 2 * j + 1;
  }
}

void benchmark_kernel(void)
{
#pragma APE_ANALYZE_BEGIN
  for (int j = 0; j < M; ++j) s[j] = 0;
  for (int i = 0; i < N; ++i)
  {
    q[i] = 0;
    for (int j = 0; j < M; ++j)
    {
      s[j] = s[j] + r[i] * A[i][j];
      q[i] = q[i] + A[i][j] * p[j];
    }
  }
#pragma APE_ANALYZE_END
}

int benchmark_validate(void)
{
  for (int j = 0; j < M; ++j)
    if (s[j] != (double)N * (2 * j + 1) + (double)N * (N - 1) / 2) return 0;
  for (int i = 0; i < N; ++i) if (q[i] != (double)M * (i + M)) return 0;
  return 1;
}
