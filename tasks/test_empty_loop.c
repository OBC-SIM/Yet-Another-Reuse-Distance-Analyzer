#include "yard_analyze.h"

#define MAX_N 100
int A[MAX_N];

YARD_ANALYZE
void test_empty_loop(void)
{
  for (int i = 0; i < MAX_N; i++)
    ;
}
