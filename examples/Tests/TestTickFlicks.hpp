#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

#include <cstdint>

namespace examples::tests
{
struct TestTickFlicks
{
  halp_meta(name, "Test tick (flicks)")
  halp_meta(c_name, "avnd_test_tick_flicks")
  halp_meta(category, "Tests/Lifecycle")
  halp_meta(uuid, "da62cdc0-3293-4ead-a759-8409af222948")
  struct { halp::val_port<"Position", float> position; } outputs;
  void operator()(halp::tick_flicks t)
  {
    outputs.position.value = static_cast<float>(t.relative_position);
  }
};
}
