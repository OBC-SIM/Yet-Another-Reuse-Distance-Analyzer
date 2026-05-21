#define MAX_SIZE 100
int array[MAX_SIZE];

void random_access_with_constant_index()
{
  array[0] = 42;
  array[1] = 42;
  array[0] = 1;
  array[2] = 84;
  array[3] = 168;
  array[4] = 336;
  array[5] = 672;
  array[1] = 42;
}