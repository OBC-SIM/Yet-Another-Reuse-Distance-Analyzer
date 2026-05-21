/**
 * PolyBench: correlation (상관관계 행렬)
 * 
 * 설명: 데이터 행렬의 열 간 상관관계 계산
 * 메모리 패턴: 
 * - 이중 루프에서 같은 배열 원소를 여러 번 접근
 * - 통계 계산 특성상 재사용 거리가 다양함
 * 
 * 실제 응용: 데이터 마이닝, 통계 분석
 */

#include <stdio.h>
#include <math.h>

#define M 30  // 데이터 포인트 수
#define N 25  // 변수 개수

double data[M][N];
double corr[N][N];
double mean[N];
double stddev[N];

void correlation_kernel() {
    int i, j, k;
    double eps = 0.1;
    
    // 1단계: 각 열의 평균 계산
    for (j = 0; j < N; j++) {
        mean[j] = 0.0;
        for (i = 0; i < M; i++) {
            mean[j] += data[i][j];  // data 열 방향 접근
        }
        mean[j] /= M;
    }
    
    // 2단계: 각 열의 표준편차 계산
    for (j = 0; j < N; j++) {
        stddev[j] = 0.0;
        for (i = 0; i < M; i++) {
            stddev[j] += (data[i][j] - mean[j]) * (data[i][j] - mean[j]);
            // data[i][j] 재접근 -> 1단계와의 RD가 큼
        }
        stddev[j] /= M;
        stddev[j] = sqrt(stddev[j]);
        stddev[j] = (stddev[j] <= eps) ? 1.0 : stddev[j];
    }
    
    // 3단계: 데이터 정규화
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            data[i][j] -= mean[j];           // mean 재사용
            data[i][j] /= sqrt(M) * stddev[j]; // stddev 재사용
        }
    }
    
    // 4단계: 상관관계 행렬 계산
    for (i = 0; i < N; i++) {
        for (j = i; j < N; j++) {
            corr[i][j] = 0.0;
            for (k = 0; k < M; k++) {
                corr[i][j] += data[k][i] * data[k][j];
                // data 재접근 -> 복잡한 재사용 패턴
            }
            corr[j][i] = corr[i][j];  // 대칭 행렬
        }
    }
}

int main() {
    int i, j;
    
    // 데이터 초기화
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            data[i][j] = (double)(i * j) / M;
        }
    }
    
    correlation_kernel();
    
    return 0;
}

