#include <stdint.h>
#include <stdlib.h>

#include "ape_analyze.h"

uint32_t rejected_array[4];
uint32_t * rejected_pointer;
_Thread_local uint32_t rejected_tls;

#if defined(YARDA_CASE_LOCAL)
APE_ANALYZE
void rejected_task(void)
{
  volatile uint32_t local_value = rejected_array[0];
  rejected_array[1] = local_value;
}
#elif defined(YARDA_CASE_POINTER)
APE_ANALYZE
void rejected_task(void) { rejected_pointer = rejected_array; }
#elif defined(YARDA_CASE_RUNTIME_INDEX)
APE_ANALYZE
void rejected_task(int index) { rejected_array[index] = 1; }
#elif defined(YARDA_CASE_TLS)
APE_ANALYZE
void rejected_task(void) { rejected_tls = 1; }
#elif defined(YARDA_CASE_HEAP)
APE_ANALYZE
void rejected_task(void)
{
  uint32_t * values = (uint32_t *)malloc(4 * sizeof(uint32_t));
  values[0] = 1;
  free(values);
}
#else
#error "one YARDA_CASE_* definition is required"
#endif

int main(void)
{
#if defined(YARDA_CASE_RUNTIME_INDEX)
  rejected_task(0);
#else
  rejected_task();
#endif
  return 0;
}
