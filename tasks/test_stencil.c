// 케이스 5: 스텐실 패턴 — arr[i-1], arr[i], arr[i+1] (offset 접근)
void stencil_1d(float out[98], float in[100])
{
  for (int i = 1; i < 99; i++) out[i - 1] = in[i - 1] + in[i] + in[i + 1];
}