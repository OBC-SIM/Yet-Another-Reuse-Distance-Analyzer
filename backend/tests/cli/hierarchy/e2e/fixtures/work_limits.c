#include <stdint.h>
#include "ape_analyze.h"

__attribute__((section(".yarda_cross"), aligned(1))) uint64_t crossing;

#ifdef EMPTY_WORK
/** @brief Reserve loop work without source emissions. @return Nothing. */
APE_ANALYZE void empty_work(void)
{
  for (int i = 0; i < 1000001; ++i) {}
}
#else
/** @brief Consume two loop entries and four line references. @return Nothing. */
APE_ANALYZE void first(void)
{
  for (int i = 0; i < 2; ++i) crossing = 1;
}

/** @brief Make all allowances except the single-loop limit cumulative.
 * @return Nothing.
 */
APE_ANALYZE void second(void)
{
  for (int i = 0; i < 3; ++i) crossing = 2;
}
#endif

/** @brief Link the image without running the workload. @return Zero. */
int main(void) { return 0; }
