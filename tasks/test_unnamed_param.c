#include "ape_analyze.h"

// 함수 정의에서 파라미터 이름이 생략되면 LLVM argument 이름이 비어 있을 수 있다.
APE_ANALYZE
void unnamed_param(int *)
{
}

int main()
{
  int value = 0;
  unnamed_param(&value);
  return 0;
}
