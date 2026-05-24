#pragma once

#if defined(__clang__)
#define YARD_ANALYZE __attribute__((annotate("yard.analyze")))
#define YARD_INLINE __attribute__((annotate("yard.inline")))
#else
#define YARD_ANALYZE
#define YARD_INLINE
#endif
