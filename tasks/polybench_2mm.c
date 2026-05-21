/**
 * PolyBench: 2mm (2 Matrix Multiplications)
 * 
 * 설명: D = A*B*C (두 번의 행렬 곱셈)
 * 메모리 패턴:
 * - 첫 번째 곱셈: tmp = A*B
 * - 두 번째 곱셈: D = tmp*C
 * - 중간 결과(tmp)의 재사용 거리 분석에 적합
 * 
 * 실제 응용: 과학 계산, 선형대수
 */

#include <stdio.h>

#define NI 16  // A의 행
#define NJ 18  // B의 열, C의 행
#define NK 20  // A의 열, B의 행
#define NL 22  // C의 열

double A[NI][NK];
double B[NK][NJ];
double C[NJ][NL];
double D[NI][NL];
double tmp[NI][NJ];  // 중간 결과

void kernel_2mm(double alpha, double beta) {
    int i, j, k;
    
    // tmp = A * B
    for (i = 0; i < NI; i++) {
        for (j = 0; j < NJ; j++) {
            tmp[i][j] = 0.0;  // tmp 첫 접근
            for (k = 0; k < NK; k++) {
                tmp[i][j] += alpha * A[i][k] * B[k][j];
                // tmp[i][j] 반복 재사용 (RD 작음)
            }
        }
    }
    
    // D = tmp * C
    for (i = 0; i < NI; i++) {
        for (j = 0; j < NL; j++) {
            D[i][j] = 0.0;
            for (k = 0; k < NJ; k++) {
                D[i][j] += beta * tmp[i][k] * C[k][j];
                // tmp 재접근 -> 첫 번째 루프와의 RD 측정
            }
        }
    }
}

int main() {
    int i, j;
    
    // 초기화
    for (i = 0; i < NI; i++) {
        for (j = 0; j < NK; j++) {
            A[i][j] = (double)(i * j) / NI;
        }
    }
    
    for (i = 0; i < NK; i++) {
        for (j = 0; j < NJ; j++) {
            B[i][j] = (double)(i * (j + 1)) / NJ;
        }
    }
    
    for (i = 0; i < NJ; i++) {
        for (j = 0; j < NL; j++) {
            C[i][j] = (double)(i * (j + 2)) / NL;
        }
    }
    
    kernel_2mm(1.5, 1.2);
    
    return 0;
}

