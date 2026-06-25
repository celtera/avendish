#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestGpuGeometryFilter
{
  halp_meta(name, "Test geometry (GPU dynamic) filter")
  halp_meta(c_name, "avnd_test_geom_gpu")
  halp_meta(category, "Tests/Geometry")
  halp_meta(uuid, "827bb0c3-11f8-4475-b363-d3e7f0b13c07")
  struct { halp::dynamic_gpu_geometry geometry; } inputs;
  struct { halp::dynamic_gpu_geometry geometry; } outputs;
  void operator()() { outputs.geometry = inputs.geometry; }
};
}
