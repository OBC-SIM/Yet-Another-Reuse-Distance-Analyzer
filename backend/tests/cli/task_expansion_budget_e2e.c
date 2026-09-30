#include "ape_analyze.h"

int expansion_anchor;

APE_INLINE
int expansion_0(int value) { return value + 1; }

#define DEFINE_EXPANSION_LEVEL(level, previous)                       \
  APE_INLINE int expansion_##level(int value)                         \
  {                                                                   \
    return expansion_##previous(value) + expansion_##previous(value); \
  }

DEFINE_EXPANSION_LEVEL(1, 0)
DEFINE_EXPANSION_LEVEL(2, 1)
DEFINE_EXPANSION_LEVEL(3, 2)
DEFINE_EXPANSION_LEVEL(4, 3)
DEFINE_EXPANSION_LEVEL(5, 4)
DEFINE_EXPANSION_LEVEL(6, 5)
DEFINE_EXPANSION_LEVEL(7, 6)
DEFINE_EXPANSION_LEVEL(8, 7)
DEFINE_EXPANSION_LEVEL(9, 8)
DEFINE_EXPANSION_LEVEL(10, 9)
DEFINE_EXPANSION_LEVEL(11, 10)
DEFINE_EXPANSION_LEVEL(12, 11)
DEFINE_EXPANSION_LEVEL(13, 12)
DEFINE_EXPANSION_LEVEL(14, 13)
DEFINE_EXPANSION_LEVEL(15, 14)
DEFINE_EXPANSION_LEVEL(16, 15)

APE_ANALYZE
void expansion_task(void) { expansion_anchor = expansion_16(1); }

int main(void)
{
  expansion_task();
  return expansion_anchor;
}
