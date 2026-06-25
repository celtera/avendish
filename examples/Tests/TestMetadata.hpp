#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestMetadata
{
  halp_meta(name, "Test metadata")
  halp_meta(c_name, "avnd_test_metadata")
  halp_meta(category, "Tests/Meta")
  halp_meta(author, "Avendish")
  halp_meta(vendor, "Avendish")
  halp_meta(product, "Test suite")
  halp_meta(version, "1.0")
  halp_meta(description, "Exercises metadata propagation")
  halp_meta(uuid, "36e08e44-5a98-4c67-abb6-3c79533491d1")
  struct { halp::val_port<"In", float> in; } inputs;
  struct { halp::val_port<"Out", float> out; } outputs;
  void operator()() { outputs.out.value = inputs.in.value; }
};
}
