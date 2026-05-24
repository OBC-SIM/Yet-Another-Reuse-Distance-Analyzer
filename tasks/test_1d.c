#include "yard_analyze.h"

// 케이스 1: 단순 1D 루프 — arr[i]
YARD_ANALYZE
void loop_1d(int arr[100])
{
  for (int i = 0; i < 100; i++) arr[i] = i;
}