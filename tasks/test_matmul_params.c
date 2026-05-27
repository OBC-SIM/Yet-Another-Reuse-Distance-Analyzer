#include "yard_analyze.h"

// 함수 파라미터 2D 배열이 pointer-to-row GEP로 분해되는 회귀 케이스.
YARD_ANALYZE
void matmul_params(float A[32][64], float B[64][32], float C[32][32])
{
  for (int i = 0; i < 32; i++)
    for (int j = 0; j < 32; j++)
      for (int k = 0; k < 64; k++) C[i][j] += A[i][k] * B[k][j];
}
