#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestCallback
{
  halp_meta(name, "Test callback")
  halp_meta(c_name, "avnd_test_callback")
  halp_meta(category, "Tests/Messages")
  halp_meta(uuid, "2c3d052e-c82e-42a9-ae6e-2ca71b9a066a")
  struct { halp::callback<"Fired", float> fired; } outputs;

  void fire(float x) { outputs.fired(x); }

  halp_start_messages(TestCallback)
    halp_mem_fun(fire)
  halp_end_messages
};
}
