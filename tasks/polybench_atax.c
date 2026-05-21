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

#define M 30  // 행렬 A의 행 수
#define N 35  // 행렬 A의 열 수

double A[M][N];
double x[N];
double y[N];
double tmp[M];  // 중간 결과 벡터

void atax_kernel() {
    int i, j;
    
    // 1단계: tmp = A * x
    for (i = 0; i < M; i++) {
        tmp[i] = 0.0;  // tmp 첫 접근
        for (j = 0; j < N; j++) {
            tmp[i] += A[i][j] * x[j];
            // A를 행 방향으로 접근
        }
    }
    
    // 2단계: y = A^T * tmp
    for (i = 0; i < N; i++) {
        y[i] = 0.0;
        for (j = 0; j < M; j++) {
            y[i] += A[j][i] * tmp[j];
            // A를 열 방향으로 접근 (캐시 미스 발생 가능)
            // tmp 재사용
        }
    }
}

int main() {
    int i, j;
    
    // 초기화
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            A[i][j] = (double)((i * j + 1) % M) / M;
        }
    }
    
    for (i = 0; i < N; i++) {
        x[i] = (double)(i % N) / N;
        y[i] = 0.0;
    }
    
    for (i = 0; i < M; i++) {
        tmp[i] = 0.0;
    }
    
    atax_kernel();
    
    return 0;
}

