static double lines[DOMAIN][4]
  __attribute__((aligned(32), section(".yarda_lines")));

/**
 * @brief Scan increasing domains while the manifest holds total accesses fixed.
 * @return Nothing; source region markers must remain in this main input file.
 */
void scan_region(void)
{
#pragma APE_ANALYZE_BEGIN
  for (int repeat = 0; repeat < REPEATS; ++repeat)
    for (int i = 0; i < DOMAIN; ++i) lines[i][0] += 1.0;
#pragma APE_ANALYZE_END
}

/** @brief Supply a linkable image without executing the benchmark.
 * @return Zero.
 */
int main(void) { return 0; }
