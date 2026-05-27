#include "yard_analyze.h"

#define M 32
#define N 32
#define K 64

float A[M][K], B[K][N], C[M][N];

// 행렬 곱셈 캐시 친화형: i-k-j 순서로 B와 C를 연속 접근한다.
YARD_ANALYZE
void matmul_cache_friendly()
{
  for (int i = 0; i < M; i++)
    for (int k = 0; k < K; k++)
      for (int j = 0; j < N; j++) C[i][j] += A[i][k] * B[k][j];
}
