#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/file_port.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestFilePort
{
  halp_meta(name, "Test file port")
  halp_meta(c_name, "avnd_test_file")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "1ec25fe5-0e0f-4051-9da6-208fbe05f03f")
  struct { halp::file_port<"In"> in; } inputs;
  struct { halp::val_port<"Loaded", bool> loaded; } outputs;
  void operator()() { outputs.loaded.value = static_cast<bool>(inputs.in); }
};
}
