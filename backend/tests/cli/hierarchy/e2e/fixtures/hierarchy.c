#include <stdint.h>
#include "ape_analyze.h"

__attribute__((section(".yarda_lines"), aligned(32))) uint32_t lines[128];
__attribute__((section(".yarda_cross"), aligned(1))) uint64_t crossing;

/** @brief Exercise independently placed conflicts in both caches.
 * @return The final load, after the same-line store.
 */
APE_ANALYZE int conflicts(void)
{
  lines[0] = 1;
  lines[0] = 2;
  lines[16] = 3;
  lines[32] = 4;
  lines[0] = 5;
  lines[64] = 6;
  lines[96] = 7;
  lines[16] = 8;
  lines[8] = 9;
  lines[0] = 10;
  return lines[0];
}

/** @brief Split each eight-byte access at offset 28 into two references.
 * @return The stored value.
 */
APE_ANALYZE uint64_t span(void)
{
  crossing = 1;
  return crossing;
}

/** @brief Start fresh even though conflicts previously touched this object.
 * @return The first cold load.
 */
APE_ANALYZE int cold(void) { return lines[0]; }

/** @brief Retain a task without references. @return Nothing. */
APE_ANALYZE void empty(void) {}

/** @brief Provide a linked image without executing analysis tasks.
 * @return Zero.
 */
int main(void) { return 0; }
