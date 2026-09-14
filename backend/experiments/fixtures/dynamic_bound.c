static double values[16];
extern int runtime_bound;

/** @brief Expose a runtime-dependent bound to the strict frontend.
 * @return Nothing; this region must be rejected before measurement.
 */
void rejected_region(void)
{
#pragma APE_ANALYZE_BEGIN
  for (int i = 0; i < runtime_bound; ++i) values[i] += 1.0;
#pragma APE_ANALYZE_END
}
