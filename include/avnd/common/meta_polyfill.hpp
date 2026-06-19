#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Detection + portable include for C++26 static reflection (P2996).
// Defines AVND_USE_STD_REFLECTION (1/0); override by predefining it.
// clang-format off
#if !defined(AVND_USE_STD_REFLECTION)
  #if defined(__cpp_reflection) || defined(__cpp_impl_reflection)
    #define AVND_USE_STD_REFLECTION 1
  #elif defined(__has_feature)
    #if __has_feature(reflection)
      #define AVND_USE_STD_REFLECTION 1
    #else
      #define AVND_USE_STD_REFLECTION 0
    #endif
  #else
    #define AVND_USE_STD_REFLECTION 0
  #endif
#endif

#if AVND_USE_STD_REFLECTION
  #if __has_include(<meta>)
    #include <meta>
  #elif __has_include(<experimental/meta>)
    #include <experimental/meta>
  #else
    // No reflection header available: disable.
    #undef AVND_USE_STD_REFLECTION
    #define AVND_USE_STD_REFLECTION 0
  #endif
#endif
// clang-format on
