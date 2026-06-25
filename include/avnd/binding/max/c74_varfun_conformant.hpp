#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Conformant-preprocessor-safe redefinition of the Max SDK's C74_VARFUN dispatch.
// No-op outside MSVC's conformant preprocessor (and on clang-cl).

#if defined(_MSVC_TRADITIONAL) && (_MSVC_TRADITIONAL == 0)

#include <ext_preprocessor.h>

#ifdef C74_VARFUN
#undef C74_VARFUN
#endif

#define C74_VARFUN_CONFORMANT_INVOKE_(N, ...) C74_JOIN_2(C74_VARFUN_, N)(__VA_ARGS__)
#define C74_VARFUN(VARFUN_IMPL, ...) \
  C74_VARFUN_CONFORMANT_INVOKE_(C74_NUM_ARGS(__VA_ARGS__), VARFUN_IMPL, __VA_ARGS__)

#endif
