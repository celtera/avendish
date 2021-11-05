#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <helpers/controls.hpp>
#include <helpers/log.hpp>
#include <helpers/meta.hpp>

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
  // $ is simply a macro that expands to a consteval function.
  // Hopefully C++ would use a similar syntax for reflexion.
  $(name, "Controls helpers")
  $(c_name, "avnd_helpers_controls")
  $(uuid, "9d356a4b-a104-4b2a-a33e-c6828135d5c6")

  // Helper types for defining common cases of UI controls
  struct
  {
    avnd::hslider_f32<"A"> a;
    avnd::knob_i32<"B", avnd::range{.min = -1000, .max = 1000, .init = 100}> b;
    avnd::toggle<"C", avnd::toggle_setup{.init = true}> c;
    avnd::lineedit<"D", "foo"> d;
    avnd::maintained_button<"E"> e;

    // The enum is a bit harder to do since we want to
    // define the enumerators and the matching strings in one go...
    // First argument after the name is the default (init) value.
    avnd__enum("Enum", Bar, Foo, Bar) f;
  } inputs;

  struct
  {
    avnd::vbargraph_f32<"A"> display;
  } outputs;

  void operator()()
  {
    if (inputs.f.value == inputs.f.Foo)
    {
      outputs.display.value = inputs.a.value + inputs.b.value;
    }
  }
};
}
