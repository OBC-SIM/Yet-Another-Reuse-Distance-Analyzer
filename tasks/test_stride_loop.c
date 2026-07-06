#include "yard_analyze.h"

#define N 128

int A[N];

YARD_ANALYZE
void stride_loop(void)
{
  for (int i = 0; i < N; i += 32)
  {
    A[i] += 1;
  }
}

int main(void)
{
  stride_loop();
  return 0;
}
