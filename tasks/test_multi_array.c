#include "ape_analyze.h"

float A[200], B[200], C[200];

// 케이스 4: 루프 내 다중 배열 접근 — A[i] = B[i] + C[i]
APE_INLINE
void saxpy(float A[200], float B[200], float C[200], float s)
{
  for (int i = 0; i < 200; i++)
    A[i] = s * B[i] + C[i];
}

APE_ANALYZE
void saxpy_kernel(void)
{
  saxpy(A, B, C, 2.0f);
}

int main()
{
  saxpy_kernel();
  return 0;
}
