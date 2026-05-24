#include "yard_analyze.h"

// 케이스 7: 로컬 배열 — alloca 기반 이름 확인
YARD_ANALYZE
void loop_local(void)
{
  int A[64][64];
  int B[64][64];
  for (int i = 0; i < 64; i++)
    for (int j = 0; j < 64; j++) A[i][j] = B[i][j] * 2;
}