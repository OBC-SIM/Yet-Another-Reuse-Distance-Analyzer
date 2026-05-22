void func_1d(float arr[64])
{
  for (int i = 0; i < 64; i++) arr[i] += 1.0f;
}

void func_2d(float A[16][16], float B[16][16])
{
  for (int i = 0; i < 16; i++)
    for (int j = 0; j < 16; j++) B[i][j] += A[i][j];
}
