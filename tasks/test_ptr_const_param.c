#include "yard_analyze.h"

#define MAX_N 100
int A[MAX_N];
int B[9];

YARD_INLINE
void test_ptr_const_param_kernel(int * ptr, const int idx)
{
  for (int i = 0; i < idx; i++) ptr[i] = 1;
}

YARD_ANALYZE
void test_ptr_const_param()
{
  B[0] = 1;
  test_ptr_const_param_kernel(A, 50);
  B[7] = 2;
  B[8] = 3;
}
