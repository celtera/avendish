#pragma once
#include <halp/controls.hpp>
#include <ossia/detail/math.hpp>

namespace ao
{
enum MapToolWrapMode
{
  Free,
  Clip,
  Shape,
  Wrap,
  Fold
};

struct MapTool
{
  halp_meta(name, "Mapping tool")
  halp_meta(c_name, "map_tool")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Map a value to a given domain in various ways")
  halp_meta(uuid, "ae6e3c9f-40cf-493a-8dc8-d45e75a07213")
  halp_flag(cv);

  struct
  {
    halp::spinbox_f32<"Min"> in_min;
    halp::spinbox_f32<"Max"> in_max;
    halp::toggle<"Learn min"> min_learn;
    halp::toggle<"Learn max"> max_learn;
    halp::enum_t<MapToolWrapMode, "Range behaviour"> range_behaviour;
    halp::knob_f32<"Curve", halp::range{-1., 1., 0.}> curve;
    halp::toggle<"Invert"> invert;
    halp::toggle<"Smooth"> smooth;

    halp::spinbox_f32<"Out min"> out_min;
    halp::spinbox_f32<"Out max"> out_max;
  } inputs;

  struct
  {
  } outputs;

  // FIXME refactor with calibrator
  static constexpr double rescale(double v, double min, double max) noexcept
  {
    return (v - min) / (max - min);
  }

  double wrap(double f)
  {
    using mode_t = MapToolWrapMode;
    switch(inputs.range_behaviour.value)
    {
      case mode_t::Free:
        return f;
      default:
      case mode_t::Clip:
        return ossia::clamp<double>(f, 0.f, 1.f);
      case mode_t::Shape:
        return (std::tanh(f * 2.f - 1.f) + 1.f) / 2.f;
      case mode_t::Wrap:
        return ossia::wrap<double>(f, 0.f, 1.f);
      case mode_t::Fold:
        return ossia::fold<double>(f, 0.f, 1.f);
    }
  }
  double operator()(double v)
  {
    /// 0. Learn min / max

    /// 1. Scale input to 0 - 1
    double in_scale = (inputs.in_max - inputs.in_min);
    if(in_scale < 1e-12f)
      return 0;

    double to_01 = rescale(v, inputs.in_min, inputs.in_max);

    /// 2. Apply operations

    // - Wrap
    to_01 = wrap(to_01);

    // - Curve
    to_01 = std::pow(to_01, inputs.curve.value + 1);

    // - Invert
    if(inputs.invert)
      to_01 = 1. - to_01;

    /// 3. Unscale
    return to_01 * (inputs.out_max - inputs.out_min) + inputs.out_min;
  }
};
}
