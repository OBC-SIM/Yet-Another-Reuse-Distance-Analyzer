#pragma once

#if defined(__clang__)
#define APE_ANALYZE __attribute__((annotate("ape.analyze")))
#define APE_INLINE __attribute__((annotate("ape.inline")))
#else
#define APE_ANALYZE
#define APE_INLINE
#endif
