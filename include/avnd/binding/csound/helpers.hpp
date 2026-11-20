#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <string_view>

// csdl.h is the plugin-opcode C API: it pulls in csoundCore.h (OPDS, MYFLT,
// STRINGDAT, the CSOUND function-pointer table, OK/NOTOK) without requiring a
// link against libcsound.
#include <csdl.h>

// interlocks.h (included by csdl.h) defines very short opcode-flag macros that
// collide with standard-library and avnd template parameters — notably _CR,
// which clobbers <chrono>'s common-ratio parameter. We never use these flags
// (opcodes are registered with flags = 0), so scrub them immediately. Every
// csound binding header includes THIS header instead of <csdl.h> directly, so
// the macros are defined and removed within this single translation step and
// can never leak into a header parsed afterwards.
#undef ZR
#undef ZW
#undef ZB
#undef WI
#undef TR
#undef TW
#undef TB
#undef _CR
#undef _CW
#undef _CB
#undef SK
#undef WR
#undef IR
#undef IW
#undef IB
#undef _QQ

namespace csound
{
/**
 * Engine pointer for the opcode currently being init'd / performed.
 * Set at the top of every init()/perf() so the logger (configure.hpp) can route
 * messages to the host. Thread-local because Csound may run instruments on
 * several worker threads.
 */
inline thread_local CSOUND* g_csound = nullptr;

inline void post(std::string_view s) noexcept
{
  if(g_csound)
    g_csound->Message(g_csound, "%.*s\n", (int)s.size(), s.data());
}

inline void bug(std::string_view s) noexcept
{
  if(g_csound)
    g_csound->ErrorMsg(g_csound, "%.*s\n", (int)s.size(), s.data());
}
}
