// 케이스 3: 행렬 곱셈 — 3중 루프, 다중 배열 접근
void matmul(float A[32][64], float B[64][32], float C[32][32])
{
  for (int i = 0; i < 32; i++)
    for (int j = 0; j < 32; j++)
      for (int k = 0; k < 64; k++) C[i][j] += A[i][k] * B[k][j];
}