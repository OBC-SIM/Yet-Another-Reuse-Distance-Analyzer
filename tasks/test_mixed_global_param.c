#include "yard_analyze.h"

float G[16][32];

// 전역 2D 배열과 함수 파라미터 2D 배열을 같은 루프에서 함께 접근한다.
YARD_ANALYZE
void mixed_global_param(float P[16][32])
{
  for (int i = 0; i < 16; i++)
    for (int j = 0; j < 32; j++) G[i][j] += P[i][j];
}
