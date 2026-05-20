// 케이스 6: 전역 배열 — 이름이 IR에 보존되는지 확인
int A[100][200];
int B[100][200];

void loop_global(void) {
    for (int i = 0; i < 100; i++)
        for (int j = 0; j < 200; j++)
            A[i][j] = B[i][j] + 1;
}
