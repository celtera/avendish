#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
// KEEPALIVE survives dead-code elimination + -fvisibility=internal and forces export
#define AVND_EXPORTED_SYMBOL EMSCRIPTEN_KEEPALIVE
#elif defined(_WIN32)
#define AVND_EXPORTED_SYMBOL __declspec(dllexport)
#else
#define AVND_EXPORTED_SYMBOL __attribute__((visibility("default")))
#endif
