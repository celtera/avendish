#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/file_port.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

#include <string>

namespace examples::tests
{
struct TestFolderPort
{
  halp_meta(name, "Test folder port")
  halp_meta(c_name, "avnd_test_folder")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "7d8d9215-229f-49d9-8feb-d7fe78a2ed50")
  struct { halp::folder_port<"In"> in; } inputs;
  struct { halp::val_port<"Out", std::string> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
