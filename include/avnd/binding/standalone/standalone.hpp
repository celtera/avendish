#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if __has_include(<ossia/detail/config.hpp>)
#if __has_include(<ossia/protocols/oscquery/oscquery_server_asio.hpp>)
#define AVND_STANDALONE_OSCQUERY 1
#include <avnd/binding/standalone/oscquery_mapper.hpp>
#endif
#endif

#if __has_include(<nuklear.h>)
#define AVND_STANDALONE_NKL 1
#include <avnd/binding/ui/nuklear_layout_ui.hpp>

#elif __has_include(<QQuickView>) && __has_include(<verdigris>)
#define AVND_STANDALONE_QML 1
#include <avnd/binding/ui/qml_ui.hpp>
#include <avnd/binding/ui/qml_layout_ui.hpp>
#endif
