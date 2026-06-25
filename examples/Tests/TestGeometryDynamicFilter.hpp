#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestGeometryDynamicFilter
{
  halp_meta(name, "Test geometry (CPU dynamic) filter")
  halp_meta(c_name, "avnd_test_geom_dyn")
  halp_meta(category, "Tests/Geometry")
  halp_meta(uuid, "287fb6d6-4380-458e-8e4b-227b7fdc3302")
  struct { halp::dynamic_geometry geometry; } inputs;
  struct { halp::dynamic_geometry geometry; } outputs;
  void operator()() { outputs.geometry = inputs.geometry; }
};
}
