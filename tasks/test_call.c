#include "yard_analyze.h"

YARD_INLINE
void touch(float x[16], int idx)
{
  x[idx] = x[idx] + 1.0f;
}

YARD_ANALYZE
void call_kernel(float a[16])
{
  for (int i = 0; i < 16; i++) touch(a, i);
}
