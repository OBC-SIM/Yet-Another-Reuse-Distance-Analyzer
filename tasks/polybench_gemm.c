/**
 * PolyBench: gemm (General Matrix Multiply)
 * 
 * 설명: C = alpha*A*B + beta*C 연산
 * 메모리 패턴: 2D 배열의 행/열 접근, 캐시 재사용 패턴이 명확함
 * 
 * 예상 메모리 패턴:
 * - C[i][j]: 매 k 반복마다 재사용 -> RD가 작음 (캐시 친화적)
 * - A[i][k]: k 반복에서 재사용
 * - B[k][j]: i 반복에서는 재사용 안됨 (캐시 비친화적)
 */

#include <stdio.h>

#define N 20  // 작은 크기로 설정 (분석 용이)

double A[N][N];
double B[N][N];
double C[N][N];

void gemm_kernel(double alpha, double beta) {
    int i, j, k;
    
    // C = alpha*A*B + beta*C
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            C[i][j] *= beta;  // 첫 접근: C[i][j]
        }
    }
    
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            for (k = 0; k < N; k++) {
                // C[i][j]는 k 루프 동안 계속 재사용됨 (RD 작음)
                // A[i][k]는 j 루프에서 재사용됨
                // B[k][j]는 i 루프에서는 재사용 안됨 (RD 큼)
                C[i][j] += alpha * A[i][k] * B[k][j];
            }
        }
    }
}

int main() {
    int i, j;
    
    // 초기화
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            A[i][j] = (double)(i * j) / N;
            B[i][j] = (double)(i * j) / N;
            C[i][j] = (double)(i * j) / N;
        }
    }
    
    gemm_kernel(1.5, 2.5);
    
    return 0;
}

