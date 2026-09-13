#include <stdint.h>
#include "ape_analyze.h"

__attribute__((section(".yarda_lines"), aligned(32))) uint32_t lines[128];

/** @brief Preserve positional object and index binding in selected loops.
 * @param values Borrowed non-null global array; ownership remains with caller.
 * @return Nothing.
 */
APE_INLINE void touch(uint32_t * values)
{
  for (int i = 1; i < 4; ++i) values[i]++;
}

/** @brief Supply a whole-function control sequence. @return Nothing. */
APE_ANALYZE void whole(void)
{
  for (int i = 1; i < 4; ++i) lines[i]++;
}

/** @brief Prefer the region over the annotation and exclude outside accesses.
 * @return Nothing.
 */
APE_ANALYZE void selected(void)
{
  const int start = 1;
  lines[1] = 99;
#pragma APE_ANALYZE_BEGIN
  for (int i = start; i < 4; ++i) lines[i]++;
#pragma APE_ANALYZE_END
  lines[1] = 100;
}

/** @brief Start another overlapping region with cold caches. @return Nothing. */
void second(void)
{
#pragma APE_ANALYZE_BEGIN
  touch(lines);
#pragma APE_ANALYZE_END
}

/** @brief Preserve selection even with no memory operations. @return Nothing. */
void empty(void)
{
#pragma APE_ANALYZE_BEGIN
#pragma APE_ANALYZE_END
}

/** @brief Link globals without executing tasks. @return Zero. */
int main(void) { return 0; }
