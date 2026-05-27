#include "yard_analyze.h"

float A[33];
float B[65];

YARD_ANALYZE
void test(float alpha)
{
  A[0] = 0.0f;
  for (int i = 1; i < 33; i++) A[i] += alpha * 0.1;
  A[0] += 1.0f;

  B[0] = 1.0f;
  for (int j = 1; j < 65; j++) B[j] += alpha * 0.2;
  B[0] += 1.1f;
}
