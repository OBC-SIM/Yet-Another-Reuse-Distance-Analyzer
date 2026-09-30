#include <stdint.h>

#include "ape_analyze.h"

struct InnerRecord
{
  uint32_t marker;
  uint64_t value;
};

struct OuterRecord
{
  uint32_t header;
  struct InnerRecord items[2];
};

__attribute__((section(".yarda_scalar"), aligned(1))) uint32_t e2e_scalar = 7;
__attribute__((section(".yarda_array"), aligned(1)))
uint32_t e2e_array[4] = {11, 13, 17, 19};
__attribute__((section(".yarda_record"),
               aligned(1))) struct OuterRecord e2e_record = {
  23, {{29, 31}, {37, 41}}};

APE_INLINE
void write_nested(struct OuterRecord * record, uint32_t values[4])
{
  for (int index = 0; index < 1; ++index)
  {
    record->items[index].value = values[index + 1];
  }
}

APE_ANALYZE
void alpha_task(void)
{
  e2e_array[0] = e2e_scalar;
  write_nested(&e2e_record, e2e_array);
}

APE_ANALYZE
void beta_task(void) { e2e_scalar = e2e_array[0]; }

int main(void)
{
  alpha_task();
  beta_task();
  return (int)e2e_record.items[0].value;
}
