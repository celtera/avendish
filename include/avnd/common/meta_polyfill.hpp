#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Centralized detection and portable include for C++26 static reflection (P2996).
//
// Defines AVND_USE_STD_REFLECTION to 1 when usable, else 0, and (when 1) includes
// the reflection header so that <std::meta> is available.
//
// Auto-detection (override by predefining AVND_USE_STD_REFLECTION):
//   - GCC 16+          : compile with -freflection ; defines __cpp_impl_reflection ;
//                        standard header <meta>
//   - clang-p2996 fork : compile with -freflection-latest ; __has_feature(reflection) ;
//                        header <experimental/meta>
//   - everything else  : 0 (no reflection)
//
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
    // Reflection was advertised but no header is available: disable it.
    #undef AVND_USE_STD_REFLECTION
    #define AVND_USE_STD_REFLECTION 0
  #endif
#endif
// clang-format on
