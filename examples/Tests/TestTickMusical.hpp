#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestTickMusical
{
  halp_meta(name, "Test tick (musical)")
  halp_meta(c_name, "avnd_test_tick_musical")
  halp_meta(category, "Tests/Lifecycle")
  halp_meta(uuid, "43d9cf1a-24ad-44fa-881e-4fb4276f31c7")
  struct
  {
    halp::val_port<"Tempo", float> tempo;
    halp::val_port<"Frames", int> frames;
  } outputs;
  void operator()(halp::tick_musical t)
  {
    outputs.tempo.value = t.tempo;
    outputs.frames.value = t.frames;
  }
};
}
