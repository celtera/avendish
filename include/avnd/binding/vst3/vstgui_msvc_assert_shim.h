#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Work around a VSTGUI header-ordering issue that only bites MSVC: for some
// translation units vstgui_assert is used (e.g. in malloc.h's Buffer<T>) before
// vstguibase.h/vstguidebug.h have defined it, giving "C3861: 'vstgui_assert':
// identifier not found". Force-including this header (see avendish.vst3.cmake)
// guarantees the macro is always defined. VSTGUI's own later definition simply
// redefines it (that C4005 warning is silenced alongside this).
#ifndef vstgui_assert
#define vstgui_assert(...) ((void)0)
#endif
