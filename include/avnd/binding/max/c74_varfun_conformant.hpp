#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// --- Max SDK C74_VARFUN compatibility shim for MSVC's conformant preprocessor ---
//
// The Max SDK implements its variadic-argument call helpers (jit_object_method,
// object_method, jit_object_new, object_new, ... -- all defined as
// `#define foo(...) C74_VARFUN(foo_imp, __VA_ARGS__)`) on top of C74_VARFUN in
// c74support/max-includes/ext_preprocessor.h. There, C74_VARFUN pastes the
// argument-count onto `C74_VARFUN_` and then appends the argument list through a
// separate macro (C74_PASS_ARGS) that injects bare `(` / `)` tokens:
//
//   #define C74_VARFUN(IMPL, ...) \
//       C74_JOIN_2(C74_VARFUN_, C74_NUM_ARGS(__VA_ARGS__)) C74_PASS_ARGS(IMPL, __VA_ARGS__)
//
// That pattern only re-triggers function-macro expansion under MSVC's *legacy*
// preprocessor. Under the conformant preprocessor (/Zc:preprocessor, which
// Avendish needs for halp's HALP_FOREACH / halp_field_names machinery), the
// pasted `C74_VARFUN_3` is not recognised as a function-like macro when the `(`
// arrives from a different expansion, so it is emitted verbatim and the compiler
// fails with "C3861: 'C74_VARFUN_3': identifier not found". clang-cl's
// preprocessor copes with both, which is why these externals build on CI (clang)
// but not with cl.exe.
//
// We can't fall back to the legacy preprocessor for this TU (it breaks halp's
// macros, e.g. C4002 on halp_field_names), so instead we redefine C74_VARFUN with
// a dispatch whose argument-list parentheses live directly in the macro body --
// the same shape halp itself uses (`HALP_FOREACH_##N(M, __VA_ARGS__)`), which the
// conformant preprocessor expands correctly.
//
// This header is force-included (-FI) ahead of the SDK headers for max externals
// built with cl.exe. It first pulls in ext_preprocessor.h to make sure the
// original C74_VARFUN / C74_VARFUN_<n> / C74_NUM_ARGS / C74_JOIN_2 macros exist,
// then overrides only C74_VARFUN. The override is guarded on the conformant
// preprocessor (_MSVC_TRADITIONAL == 0), so it is a complete no-op under the
// legacy preprocessor and under clang-cl (which does not define _MSVC_TRADITIONAL).

#if defined(_MSVC_TRADITIONAL) && (_MSVC_TRADITIONAL == 0)

// Ensure the SDK's variadic-call machinery is defined before we patch it. This is
// a no-op when ext.h / jit.common.h already pulled it in (header guarded); it only
// defines macros, so it is safe to include before the rest of the SDK (the
// C74_VARFUN_<n> bodies cast to t_ptr_int, but that is only evaluated at the call
// site, by which point ext.h has defined the type).
#include <ext_preprocessor.h>

#ifdef C74_VARFUN
#undef C74_VARFUN
#endif

// One level of indirection so C74_NUM_ARGS(__VA_ARGS__) is fully expanded to a
// digit before it is pasted onto `C74_VARFUN_`. The argument list `(__VA_ARGS__)`
// is a literal token sequence in the macro body, so the conformant preprocessor
// recognises `C74_VARFUN_<n>( ... )` as a function-macro invocation on rescan.
#define C74_VARFUN_CONFORMANT_INVOKE_(N, ...) C74_JOIN_2(C74_VARFUN_, N)(__VA_ARGS__)
#define C74_VARFUN(VARFUN_IMPL, ...) \
  C74_VARFUN_CONFORMANT_INVOKE_(C74_NUM_ARGS(__VA_ARGS__), VARFUN_IMPL, __VA_ARGS__)

#endif // _MSVC_TRADITIONAL == 0
