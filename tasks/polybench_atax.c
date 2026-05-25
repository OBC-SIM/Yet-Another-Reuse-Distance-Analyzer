#include "yard_analyze.h"

/**
 * PolyBench: atax (Matrix Transpose and Vector Multiplication)
 *
 * 설명: y = A^T * (A * x) 계산
 * 메모리 패턴:
 * - 행렬 A를 두 번 접근 (다른 방향)
 * - 임시 벡터 tmp의 재사용
 *
 * 실제 응용: 선형 시스템, 최소제곱법
 */

#include <stdio.h>

#define M 150  // 행렬 A의 행 수
#define N 70   // 행렬 A의 열 수

double A[M][N];
double x[N];
double y[N];
double tmp[M];  // 중간 결과 벡터

YARD_INLINE
void atax_kernel()
{
  int i, j;

  for (i = 0; i < N; i++) y[i] = 0;
  for (i = 0; i < M; i++)
  {
    tmp[i] = 0.0;
    for (j = 0; j < N; j++)
    {
      tmp[i] = tmp[i] + A[i][j] * x[j];
    }
    for (j = 0; j < N; j++) y[j] = y[j] + A[i][j] * tmp[i];
  }
}

YARD_ANALYZE
int main()
{
  int i, j;

  // 초기화
  for (i = 0; i < M; i++)
  {
    for (j = 0; j < N; j++)
    {
      A[i][j] = (double)((i * j + 1) % M) / M;
    }
  }

  for (i = 0; i < N; i++)
  {
    x[i] = (double)(i % N) / N;
    y[i] = 0.0;
  }

  for (i = 0; i < M; i++)
  {
    tmp[i] = 0.0;
  }

  atax_kernel();

  return 0;
}
