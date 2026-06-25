#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestTick
{
  halp_meta(name, "Test tick")
  halp_meta(c_name, "avnd_test_tick")
  halp_meta(category, "Tests/Lifecycle")
  halp_meta(uuid, "859afa1c-1934-477d-9429-1f3aab1596ff")
  struct { halp::val_port<"Frames", int> frames; } outputs;
  void operator()(halp::tick t) { outputs.frames.value = t.frames; }
};
}
