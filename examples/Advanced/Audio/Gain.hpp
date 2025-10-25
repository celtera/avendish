#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/meta.hpp>
#include <halp/smooth_controls.hpp>

namespace ao
{
class Gain
{
public:
  halp_meta(name, "Gain")
  halp_meta(c_name, "Gain")
  halp_meta(category, "Audio/Utilities")
  halp_meta(author, "ossia score")
  halp_meta(description, "A simple volume control")
  halp_meta(uuid, "6c158669-0f81-41c9-8cc6-45820dcda867")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#gain")

  struct inputs
  {
    halp::smooth_knob<"Gain", halp::range{0., 5., 0.}> gain;
  };
  double operator()(double input, const inputs& in) { return input * in.gain; }
};
}
