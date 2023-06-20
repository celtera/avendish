#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

namespace ao
{

static constexpr halp::range_slider_range mapping_range{
    .min = -1e5, .max = 1e5, .init = {0, 1}};
struct Mapping
{
public:
  halp_meta(name, "Mapping")
  halp_meta(c_name, "mapping")
  halp_meta(category, "Mappings")
  halp_meta(description, "Mapping curve")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "3fa0eb4b-f280-420a-9674-c2696ddf17ff")

  struct
  {
    halp::val_port<"In", double> value;
    halp::curve_port<"Curve"> curve;

    halp::range_spinbox_f32<"In range", mapping_range> xrange;
    halp::range_spinbox_f32<"Out range", mapping_range> yrange;
  } inputs;

  struct
  {
    halp::val_port<"Out", double> value;
  } outputs;

  void operator()() noexcept
  {
    outputs.value = inputs.curve.value.value_at(inputs.value);
  }
};
}
