#include "yard_analyze.h"

#define MAX_SIZE 100
int array[MAX_SIZE];

YARD_ANALYZE
void random_access_with_constant_index()
{
  array[0] = 42;
  array[0] = 1;
  array[1] = 84;
  array[2] = 168;
  array[1] = 336;
  array[2] = 672;
  array[0] = 42;
  array[0] = 42;
}