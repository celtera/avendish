#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestInitialize
{
  halp_meta(name, "Test initialize")
  halp_meta(c_name, "avnd_test_initialize")
  halp_meta(category, "Tests/Lifecycle")
  halp_meta(uuid, "af47f643-cc52-4b07-97fa-7d9398ad3fb0")
  struct { halp::val_port<"Initialized", bool> initialized; } outputs;
  bool m_initialized = false;
  void initialize() { m_initialized = true; }
  void operator()() { outputs.initialized.value = m_initialized; }
};
}
