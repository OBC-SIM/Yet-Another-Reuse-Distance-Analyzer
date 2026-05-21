void test(float* A, float alpha) {
    A[0] = 0.0f;
    for (int i = 0; i < 100; i++)
        A[i] += alpha;
    A[0] += 1.0f;
}
