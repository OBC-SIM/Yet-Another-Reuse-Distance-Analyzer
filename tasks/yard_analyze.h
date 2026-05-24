#pragma once

#if defined(__clang__)
#define YARD_ANALYZE __attribute__((annotate("yard.analyze")))
#else
#define YARD_ANALYZE
#endif
