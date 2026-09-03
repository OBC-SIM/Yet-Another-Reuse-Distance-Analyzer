#include "yard_analyze.h"

int arr[64];
float A[16][16], B[16][16];

YARD_INLINE
void func_1d(int arr[64])
{
  for (int i = 0; i < 64; i++)
    arr[i] += 1;
}

YARD_ANALYZE
void func_1d_kernel(void)
{
  func_1d(arr);
}

YARD_INLINE
void func_2d(float A[16][16], float B[16][16])
{
  for (int i = 0; i < 16; i++)
    for (int j = 0; j < 16; j++)
      B[i][j] += A[i][j];
}

YARD_ANALYZE
void func_2d_kernel(void)
{
  func_2d(A, B);
}

int main()
{
  func_1d_kernel();
  func_2d_kernel();
  return 0;
}
