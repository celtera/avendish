#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestEnum
{
  halp_meta(name, "Test enum")
  halp_meta(c_name, "avnd_test_enum")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "deda7ca2-889f-488b-9313-0e62a13db3fd")
  struct { struct { halp__enum("Mode", Bar, Foo, Bar, Baz) } mode; } inputs;
  struct { halp::val_port<"Index", int> index; } outputs;
  void operator()() { outputs.index.value = static_cast<int>(inputs.mode.value); }
};
}
