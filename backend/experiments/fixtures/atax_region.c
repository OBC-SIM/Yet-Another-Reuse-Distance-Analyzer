/* PolyBench/C 4.2.1 ATAX operation, adapted to linked static storage.
 * Dataset definitions: MINI M=38/N=42; SMALL M=116/N=124.
 * The original array allocation, initialization and printing are excluded.
 * Compound assignment matches the independently checked B11 2x3 fixture.
 */
static double A[M][N] __attribute__((aligned(32), section(".yarda_A")));
static double x[N] __attribute__((aligned(32), section(".yarda_x")));
static double y[N] __attribute__((aligned(32), section(".yarda_y")));
static double tmp[M] __attribute__((aligned(32), section(".yarda_tmp")));

/**
 * @brief Analyze the bounded ATAX computation with explicitly placed objects.
 * @return Nothing; input preparation is outside the selected cold region.
 */
void atax_region(void)
{
#pragma APE_ANALYZE_BEGIN
  for (int i = 0; i < N; ++i) y[i] = 0;
  for (int i = 0; i < M; ++i)
  {
    tmp[i] = 0;
    for (int j = 0; j < N; ++j) tmp[i] += A[i][j] * x[j];
    for (int j = 0; j < N; ++j) y[j] += A[i][j] * tmp[i];
  }
#pragma APE_ANALYZE_END
}

/** @brief Supply a linkable image without executing the benchmark.
 * @return Zero.
 */
int main(void) { return 0; }
