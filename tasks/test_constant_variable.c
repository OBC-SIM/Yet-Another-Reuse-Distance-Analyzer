#include "yard_analyze.h"

#define MAX_N 100
int A[MAX_N];

YARD_ANALYZE
void test_constatnt_variable(void)
{
  const int N = 50;

  for (int i = 0; i < N; i++) A[i] = 1;
}
