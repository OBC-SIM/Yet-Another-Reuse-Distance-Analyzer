#include "ape_analyze.h"

int data[32];

/** @brief Retain a known non-inline callee for the call rejection case.
 * @return Nothing.
 */
APE_ANALYZE void opaque(void) { data[0] = 1; }

/** @brief Select one deliberately unsupported source construct.
 * @param n Runtime bound/index, unresolved by the strict frontend.
 * @return Nothing.
 */
#ifdef INLINE_REGION
APE_INLINE
#endif
void selected(int n)
{
#if defined(REVERSED)
#pragma APE_ANALYZE_END
#pragma APE_ANALYZE_BEGIN
#elif defined(MISSING_END)
#pragma APE_ANALYZE_BEGIN
  data[0] = 1;
#elif defined(NESTED)
#pragma APE_ANALYZE_BEGIN
#pragma APE_ANALYZE_BEGIN
  data[0] = 1;
#pragma APE_ANALYZE_END
#pragma APE_ANALYZE_END
#elif defined(PARTIAL_LOOP)
  for (int i = 0; i < 3; ++i)
  {
#pragma APE_ANALYZE_BEGIN
    data[i] = 1;
  }
#pragma APE_ANALYZE_END
#else
#pragma APE_ANALYZE_BEGIN
#if defined(SCALED)
  for (int i = 0; i < 3; ++i) data[2 * i] = 1;
#elif defined(RUNTIME_BOUND)
  for (int i = 0; i < n; ++i) data[i] = 1;
#elif defined(RUNTIME_INDEX)
  data[n] = 1;
#elif defined(CONTROL_FLOW)
  if (n) data[0] = 1;
#elif defined(RETURN_INSIDE)
  return;
#elif defined(OPAQUE_REGION)
  opaque();
#else
  data[0] = 1;
#endif
#pragma APE_ANALYZE_END
#endif
}

/** @brief Supply ET_EXEC linkage without executing rejected code. @return Zero. */
int main(void) { return 0; }
