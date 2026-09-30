#include "ape_analyze.h"

// 케이스 2: 2D 중첩 루프 — A[i][j]

int A[64][64];

APE_INLINE
void loop_2d(int A[64][64])
{
  for (int i = 0; i < 64; i++)
  {
    for (int j = 0; j < 64; j++)
      A[i][j] += i + j;
  }
}

APE_ANALYZE
void loop_2d_kernel(void) { loop_2d(A); }

int main()
{
  loop_2d_kernel();
  return 0;
}
