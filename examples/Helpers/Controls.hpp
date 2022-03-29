#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/log.hpp>
#include <halp/meta.hpp>

#include <cstdio>

namespace examples::helpers
{
/**
 * This examples shows how things can look with a small helper
 * library to abstract common use cases.
 *
 * It will produce an object that can be "banged" in environments
 * where this is meaningful
 */
struct Controls
{
  // avnd_meta is simply a macro that expands to a consteval function.
  // Hopefully C++ would use a similar syntax for reflexion.
  avnd_meta(name, "Controls helpers")
  avnd_meta(c_name, "avnd_helpers_controls")
  avnd_meta(uuid, "9d356a4b-a104-4b2a-a33e-c6828135d5c6")

  // Helper types for defining common cases of UI controls
  struct
  {
    halp::hslider_f32<"A"> a;
    halp::knob_i32<"B", halp::irange{.min = -1000, .max = 1000, .init = 100}> b;
    halp::toggle<"C", halp::toggle_setup{.init = true}> c;
    halp::lineedit<"D", "foo"> d;
    halp::maintained_button<"E"> e;

    // The enum is a bit harder to do since we want to
    // define the enumerators and the matching strings in one go...
    // First argument after the name is the default (init) value.
    halp__enum("Enum", Bar, Foo, Bar) f;
  } inputs;

  struct
  {
    halp::vbargraph_f32<"A"> display;
  } outputs;

  void operator()()
  {
    if (inputs.f == inputs.f.Foo)
    {
      outputs.display = inputs.a + inputs.b;
    }
  }
};
}
