volatile int ir_values[3];

__attribute__((annotate("ape.inline"), noinline))
int kernel(void)
{
  int sum = 0;
  for (int i = 0; i < 3; ++i) sum += ir_values[i];
  return sum;
}

__attribute__((annotate("ape.analyze"), noinline))
void job(void)
{
  for (int sweep = 0; sweep < 7; ++sweep) kernel();
}

int main(void)
{
  job();
  return 0;
}
