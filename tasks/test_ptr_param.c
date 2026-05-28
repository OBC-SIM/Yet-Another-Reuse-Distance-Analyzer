#include "yard_analyze.h"

#define MAX_N 100
int A[MAX_N];
int B[9];

YARD_INLINE
void test_ptr_param_kernel(int * ptr, int idx) { ptr[idx] = 1; }

YARD_ANALYZE
void test_ptr_param()
{
  B[0] = 1;
  for (int i = 0; i < 50; i++) test_ptr_param_kernel(A, i);
  B[8] = 4;
  B[7] = 2;
  B[8] = 3;
}
