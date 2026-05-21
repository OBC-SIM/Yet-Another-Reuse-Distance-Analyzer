/**
 * PolyBench: jacobi-2d (Jacobi Stencil Computation)
 * 
 * 설명: 2D 격자에서 반복적인 평균 계산 (편미분방정식 수치해석)
 * 메모리 패턴:
 * - Stencil 패턴: 주변 4개 점의 평균
 * - 시간 단계마다 배열 교환
 * - 공간 지역성이 높은 패턴
 * 
 * 실제 응용: 물리 시뮬레이션, 이미지 처리
 */

#include <stdio.h>

#define N 40      // 격자 크기
#define TSTEPS 5  // 시간 단계 수 (작게 설정)

double A[N][N];
double B[N][N];

void jacobi_2d_kernel() {
    int t, i, j;
    
    for (t = 0; t < TSTEPS; t++) {
        // A -> B 업데이트
        for (i = 1; i < N - 1; i++) {
            for (j = 1; j < N - 1; j++) {
                // 5-point stencil: 중앙, 상, 하, 좌, 우
                B[i][j] = 0.2 * (A[i][j]     +  // 중앙
                                A[i][j-1]   +  // 좌
                                A[i][j+1]   +  // 우
                                A[i-1][j]   +  // 상
                                A[i+1][j]);    // 하
                // A의 인접 원소들이 연속적으로 접근됨 (공간 지역성)
            }
        }
        
        // B -> A 업데이트
        for (i = 1; i < N - 1; i++) {
            for (j = 1; j < N - 1; j++) {
                A[i][j] = 0.2 * (B[i][j]     +
                                B[i][j-1]   +
                                B[i][j+1]   +
                                B[i-1][j]   +
                                B[i+1][j]);
                // B 재사용 -> 이전 단계와의 RD 측정
            }
        }
    }
}

int main() {
    int i, j;
    
    // 초기화
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            A[i][j] = (double)((i * (j + 2) + 2) % N) / N;
            B[i][j] = (double)((i * (j + 3) + 3) % N) / N;
        }
    }
    
    jacobi_2d_kernel();
    
    return 0;
}

