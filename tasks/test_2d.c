// 케이스 2: 2D 중첩 루프 — A[i][j]
void loop_2d(int A[64][64]) {
    for (int i = 0; i < 64; i++)
        for (int j = 0; j < 64; j++)
            A[i][j] = i + j;
}
