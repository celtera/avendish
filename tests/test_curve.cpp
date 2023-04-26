#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <smallfun_trivial.hpp>

namespace ao
{
using func = float (*)(float x); // in / out between [0; 1]

struct functable
{
  func linear;
  func pow;
};

// Option 1
struct curve_segment
{
  struct
  {
    float x{}, y{};
  } start;

  struct
  {
    float x{}, y{};
  } end;

  float gamma{1.0};
};

struct curve : std::vector<curve_segment>
{
  float value_at(float position)
  {
    // 1. Find the segment we are in
    if(this->empty())
      return 0.;

    auto it = std::upper_bound(
        this->begin(), this->end(), position,
        [](float v, const curve_segment& segt) noexcept { return v < segt.start.x; });

    if(it != this->begin())
      --it;

    auto& segment = *it;
    if(segment.end.x - segment.start.x < 1e-8f)
      return segment.start.y; // FIXME with a pow function?

    // 2. Rescale global position to position in the segment.
    // spos is in [0; 1].
    const float spos = (position - segment.start.x) / (segment.end.x - segment.start.x);

    // 3. Apply the mapping function (in [0; 1] -> [0; 1])
    const float smap = std::pow(spos, segment.gamma);

    // 4. Scale the result into the y axis
    const float sval = segment.start.y + smap * (segment.end.y - segment.start.y);

    return sval;
  }
};

// Option 2
using function_t
    = smallfun::trivial_function<float(float), 3 * sizeof(float), alignof(float)>;
struct curve_segment_2
{
  float start{};
  float end{};

  function_t function{};
};

struct curve2 : std::vector<curve_segment_2>
{
  float value_at(float position)
  {
    // 1. Find the segment we are in
    if(this->empty())
      return 0.;

    auto it = std::upper_bound(
        this->begin(), this->end(), position,
        [](float v, const curve_segment_2& segt) noexcept { return v < segt.start; });

    if(it != this->begin())
      --it;

    auto& segment = *it;
    if(segment.end - segment.start < 1e-8f)
      return segment.function(segment.start);

    // 2. Rescale global position to position in the segment.
    // spos is in [0; 1].
    const float spos = (position - segment.start) / (segment.end - segment.start);

    // 2. Apply the mapping function (in [0; 1] -> [min; max])
    return segment.function(spos);
  }
};
struct Automation
{
public:
  halp_meta(name, "Automation")
  halp_meta(c_name, "automation")
  halp_meta(category, "Automation/Float")
  halp_meta(description, "An automation curve")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "494bd8a3-e973-4fb0-b84b-b4ed3c0068a1")

  struct
  {
    struct
    {
      curve curve;
    } automation1;
    struct
    {
      curve2 curve;
    } automation2;

  } inputs;

  struct
  {
    struct
    {
      float value{};
    } out;
    struct
    {
      float value{};
    } out2;
  } outputs;

  void prepare(halp::setup info) noexcept { }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical frames) noexcept
  {
    outputs.out.value
        = inputs.automation1.curve.value_at(frames.position_in_frames / (48000 * 10.));
    outputs.out2.value
        = inputs.automation2.curve.value_at(frames.position_in_frames / (48000 * 10.));
  }
};
}
