#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if defined(_WIN32)
#define AVND_EXPORTED_SYMBOL __declspec(dllexport)
#else
#define AVND_EXPORTED_SYMBOL __attribute__((visibility("default")))
#endif
