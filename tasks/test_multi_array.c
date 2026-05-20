// 케이스 4: 루프 내 다중 배열 접근 — A[i] = B[i] + C[i]
void saxpy(float A[200], float B[200], float C[200], float s) {
    for (int i = 0; i < 200; i++)
        A[i] = s * B[i] + C[i];
}
