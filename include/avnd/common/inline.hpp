#pragma once

#if defined(__clang__)
#define AVND_INLINE inline __attribute__((always_inline))
#elif defined(__GNUC__)
#define AVND_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define AVND_INLINE inline __forceinline
#else
#define AVND_INLINE inline
#endif

#if defined __has_attribute
#if __has_attribute(flatten)
#define AVND_INLINE_FLATTEN AVND_INLINE __attribute__((flatten))
#endif
#endif

#if !defined(AVND_INLINE_FLATTEN)
#define AVND_INLINE_FLATTEN AVND_INLINE
#endif
