#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <cstdint>

namespace avnd
{

/**
 * Severity of a condition an object reports about itself.
 *
 * The vocabulary follows OpenFX's message suite, which draws the distinctions
 * most precisely:
 *  - info:    conveys information to the user; not a problem
 *  - warning: proceeds, but not optimally
 *  - error:   recoverable by user intervention
 *  - fatal:   the object can no longer operate
 *
 * Developer-facing tracing is *not* part of this: that is what avnd::logger is
 * for. OFX draws the same line between "Message" and "Log".
 */
enum class diagnostic_severity : uint8_t
{
  info = 0,
  warning = 1,
  error = 2,
  fatal = 3
};

/**
 * Objects opt in by declaring a `diagnostics` member (see halp::diagnostics):
 *
 *   struct MyObject {
 *     halp::diagnostics diagnostics;
 *     void operator()() {
 *       if(!connected) diagnostics.warning("no sender on port {}", port);
 *     }
 *   };
 *
 * Diagnostics are scoped to one run of the object: the binding clears them
 * before invoking it, so an object simply states what is true now and stops
 * stating what no longer is. Nothing to clear, no identifiers to manage.
 */
template <typename T>
concept has_diagnostics = requires(T t) { t.diagnostics.entries; };

}
