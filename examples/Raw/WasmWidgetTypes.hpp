#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/controls.sliders.hpp>
#include <halp/meta.hpp>

#include <vector>

namespace examples
{
// Exercises the compound widget types for the WASM control-bridge test matrix:
// xy, xyz, color (rgba), range slider, multi_slider, enum, combobox.
struct WasmWidgetTypes
{
  halp_meta(name, "WasmWidgetTypes")
  halp_meta(c_name, "avnd_wasm_widget_types")
  halp_meta(uuid, "0e4d2a11-3c5b-4a90-9f01-2c8e7b6d4f10")

  enum class Mode
  {
    Low,
    Mid,
    High
  };

  struct
  {
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain;
    halp::knob_i32<"Count", halp::irange{.min = 0, .max = 16, .init = 4}> count;
    halp::toggle<"Bypass"> bypass;
    halp::lineedit<"Label", "hello"> label;

    halp::xy_pad_f32<"XY", halp::range{.min = -1., .max = 1., .init = 0.}> xy;
    halp::xyz_spinboxes_f32<"XYZ", halp::range{.min = -10., .max = 10., .init = 0.}> xyz;
    halp::color_chooser<"Color"> color;
    halp::range_slider_f32<"Range"> range;

    struct
    {
      halp_meta(name, "Mode");
      enum widget
      {
        enumeration
      };
      Mode value{Mode::Mid};
    } mode;

    struct
    {
      halp_meta(name, "Combo");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<float> values[3]{{"A", 0.1f}, {"B", 0.5f}, {"C", 0.9f}};
        int init{1};
      };
      float value{};
    } combo;

    struct
    {
      halp_meta(name, "Multi");
      enum widget
      {
        multi_slider
      };
      std::vector<float> value{0.1f, 0.2f, 0.3f};
    } multi;
  } inputs;

  struct
  {
    halp::hbargraph_f32<"Meter", halp::range{.min = 0., .max = 1., .init = 0.}> meter;
    halp::val_port<"OutValue", float> out_value;
  } outputs;

  void operator()() { outputs.meter = inputs.gain.value / 2.f; }
};
}
