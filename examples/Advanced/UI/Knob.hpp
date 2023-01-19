#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace oscr::uis
{
class Knob
{
public:
  halp_meta(name, "Knob")
  halp_meta(c_name, "avnd_ui_knob")
  halp_meta(category, "UI")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Basic UI knob")
  halp_meta(uuid, "209b71d0-d7ee-49d1-ba79-557e32ef4888")

  struct
  {
    struct : halp::knob_f32<"Value", halp::range{0., 1., 0.5}>
    {
      void update(Knob& self) { self.outputs.out.value = this->value; }
    } pos;
  } inputs;

  struct
  {
    halp::val_port<"Output", float> out;
  } outputs;

  void operator()() { }
};
}
