#include "ape_analyze.h"

#ifndef GAP_LINES
#define GAP_LINES 1024
#endif

#ifndef REPS
#define REPS 16
#endif

#define LINE_FLOATS 8  // 32 B cache line / 4 B float

float target[LINE_FLOATS];
float gap[GAP_LINES][LINE_FLOATS];

/*
 * Synthetic reuse-distance threshold scenario.
 *
 * Each iteration accesses target[0], touches GAP_LINES distinct cache lines,
 * and then reuses target[0].  Increasing GAP_LINES increases the cache-line
 * reuse distance, while a cache hierarchy maps those distances to L1, LLC, or
 * memory-level behavior.
 */
APE_ANALYZE
void rd_cache_threshold_kernel(void)
{
  for (int r = 0; r < REPS; r++)
  {
    target[0] += 1.0f;
    for (int i = 0; i < GAP_LINES; i++)
    {
      gap[i][0] += 1.0f;
    }
    target[0] += 1.0f;
  }
}

int main(void)
{
  rd_cache_threshold_kernel();
  return 0;
}
