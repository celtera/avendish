#pragma once

#if defined(__GNUC__)
#define HALP_INLINE inline __attribute__((always_inline))
#elif defined(__clang__)
#define HALP_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define HALP_INLINE inline __forceinline
#else
#define HALP_INLINE inline
#endif

#if defined(__clang__)
#define HALP_INLINE_FLATTEN HALP_INLINE __attribute__((flatten))
#else
#define HALP_INLINE_FLATTEN HALP_INLINE
#endif
