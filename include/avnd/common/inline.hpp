#pragma once

#if defined(__GNUC__)
#define AVND_INLINE inline __attribute__((always_inline))
#elif defined(__clang__)
#define AVND_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define AVND_INLINE inline __forceinline
#else
#define AVND_INLINE inline
#endif

#if defined(__clang__)
#define AVND_INLINE_FLATTEN AVND_INLINE __attribute__((flatten))
#else
#define AVND_INLINE_FLATTEN AVND_INLINE
#endif
