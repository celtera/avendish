#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/diagnostics.hpp>
#include <halp/meta.hpp>

namespace examples::helpers
{
/**
 * Reporting conditions to the host.
 *
 * Unlike the logger, which is a stream of lines for the developer, diagnostics
 * are the object's current state, shown by the host on the node itself:
 * TouchDesigner puts the node into a warning or error state, Max and Pd post to
 * the console when it changes, and so on.
 *
 * The set is cleared before every run, so state what is true now and stop
 * stating what is not.
 */
struct Diagnostics
{
  halp_meta(name, "Diagnostics")
  halp_meta(c_name, "avnd_helpers_diagnostics")
  halp_meta(category, "Demo")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Reporting errors, warnings and info to the host")
  halp_meta(uuid, "8f6dc0a0-4c8a-4b16-9d47-2b4b3a8f1c02")

  // The entire opt-in.
  halp::diagnostics diagnostics;

  struct
  {
    halp::hslider_f32<"Level", halp::range{0., 1., 0.5}> level;
    halp::spinbox_i32<"Count", halp::range{0, 1000, 10}> count;
  } inputs;

  struct
  {
    halp::val_port<"Out", float> out;
  } outputs;

  void operator()()
  {
    // Plain messages: no identifier needed.
    if(inputs.count == 0)
      diagnostics.warning("nothing to process");

    // With a stable identifier, for hosts that suppress or localise by id, for
    // structured codes, and so tests can assert on the condition rather than on
    // the wording.
    if(inputs.level > 0.95f)
      diagnostics.error<"level_too_high">("level {:.2f} is out of range", inputs.level.value);

    diagnostics.info("processing {} items", inputs.count.value);

    outputs.out = inputs.level * inputs.count;
  }
};
}
