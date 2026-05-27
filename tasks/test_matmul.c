#include "yard_analyze.h"

#define M 32
#define N 32
#define K 64

float A[M][K], B[K][N], C[M][N];

// 케이스 3: 행렬 곱셈 — 3중 루프, 다중 배열 접근
YARD_ANALYZE
void matmul()
{
  for (int i = 0; i < M; i++)
    for (int j = 0; j < N; j++)
      for (int k = 0; k < K; k++) C[i][j] += A[i][k] * B[k][j];
}
