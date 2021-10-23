#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if __has_include(<ossia/detail/config.hpp>)
#define AVND_STANDALONE_OSCQUERY 1
#include <standalone/oscquery_mapper.hpp>
#endif

#if __has_include(<QQuickView>) && __has_include(<verdigris>)
#define AVND_STANDALONE_QML 1
#include <ui/qml_ui.hpp>
#endif
