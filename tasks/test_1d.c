#include "ape_analyze.h"

int arr[100];

// 케이스 1: 단순 1D 루프 — arr[i]
APE_INLINE
void loop_1d(int arr[100])
{
  for (int i = 0; i < 100; i++)
    arr[i] = i;
}

APE_ANALYZE
void loop_1d_kernel(void)
{
  loop_1d(arr);
}

int main()
{
  loop_1d_kernel();
  return 0;
}
