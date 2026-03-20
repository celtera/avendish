#pragma once
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>

#include <functional>

namespace ao
{
enum MapToolWrapMode
{
  Free,
  Clip,
  Wrap,
  Fold,
};
enum MapToolShapeMode
{
  None,
  Tanh,
  Sin,
  Asym
};

// FIXME refactor with calibrator
static constexpr double rescale(double v, double min, double max) noexcept
{
  return (v - min) / (max - min);
}

static inline double wrap(MapToolWrapMode mode, double f)
{
  using mode_t = MapToolWrapMode;
  switch(mode)
  {
    case mode_t::Free:
      return f;
    default:
    case mode_t::Clip:
      return ossia::clamp<double>(f, 0., 1.);
    case mode_t::Wrap:
      return ossia::wrap<double>(f, 0., 1.);
    case mode_t::Fold:
      return ossia::fold<double>(f, 0., 1.);
  }
}

static inline double shape(MapToolShapeMode mode, double f)
{
  using mode_t = MapToolShapeMode;
  switch(mode)
  {
    default:
    case mode_t::None:
      return f;
    case mode_t::Tanh:
      return (std::tanh(f * 2. - 1.) + 1.) / 2.;
    case mode_t::Sin:
      return (std::sin(f * 2. - 1.) + 1.) / 2.;
    case mode_t::Asym: {
      double x = f * 2. - 1.;
      double shape = (std::exp(x) - std::exp(-x * 1.2)) / (std::exp(x) + std::exp(-x));
      return (shape + 1.) / 2.;
    }
  }
}
struct MapTool
{
  halp_meta(name, "Mapping tool")
  halp_meta(c_name, "map_tool")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Map a value to a given domain in various ways")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/mapping-tool.html")
  halp_meta(uuid, "ae6e3c9f-40cf-493a-8dc8-d45e75a07213")
  halp_flag(cv);
  halp_flag(stateless);

  struct ins
  {
    struct : halp::spinbox_f32<"Min", halp::free_range_min<>>
    {
      halp_flag(class_attribute);
      std::function<void(float)> update_controller;
    } in_min;
    struct : halp::spinbox_f32<"Max", halp::free_range_max<>>
    {
      halp_flag(class_attribute);
      std::function<void(float)> update_controller;
    } in_max;

    halp::spinbox_f32<"Midpoint", halp::free_range_min<>> midpoint;
    halp::spinbox_f32<"Deadzone", halp::positive_range_min<>> deadzone;

    struct : halp::toggle<"Learn min">
    {
      void update(MapTool& self)
      {
        if(value)
        {
          self.inputs.in_min.value = std::numeric_limits<float>::max();
        }
      }
    } min_learn;
    struct : halp::toggle<"Learn max">
    {
      void update(MapTool& self)
      {
        if(value)
        {
          self.inputs.in_max.value = std::numeric_limits<float>::lowest();
        }
      }
    } max_learn;

    halp::enum_t<MapToolWrapMode, "Range behaviour"> range_behaviour;
    halp::enum_t<MapToolShapeMode, "Shape behaviour"> shape_behaviour;
    halp::knob_f32<"Curve", halp::range{-1., 1., 0.}> curve;
    halp::toggle<"Invert"> invert;
    halp::toggle<"Absolute value"> absolute;

    halp::spinbox_f32<"Out min", halp::free_range_min<>> out_min;
    halp::spinbox_f32<"Out max", halp::free_range_max<>> out_max;
    // TODO add a gain (* x) & boost (+ x) setting ?
  } inputs;

  struct
  {
  } outputs;

  double operator()(double v)
  {
    static constexpr double float_min = std::numeric_limits<float>::min();
    /// Learn min / max
    // TODO think of a better way to have host feature detection?
    if(inputs.in_min.update_controller)
    {
      if(inputs.min_learn && v < inputs.in_min)
      {
        inputs.in_min.value = v;
        inputs.in_min.update_controller(v);
      }
      if(inputs.max_learn && v > inputs.in_max)
      {
        inputs.in_max.value = v;
        inputs.in_max.update_controller(v);
      }
    }

    /// Absolute value
    if(inputs.absolute)
    {
      v = std::abs(v);
    }
    if(std::abs(v - inputs.midpoint) < inputs.deadzone)
      v = inputs.midpoint;

    /// Scale input to 0 - 1
    const double in_scale = (inputs.in_max - inputs.in_min);
    if(in_scale < float_min)
      return 0.5 * (inputs.out_max - inputs.out_min) + inputs.out_min;

    double to_01 = rescale(v, inputs.in_min, inputs.in_max);

    /// Apply operations

    // - Wrap
    to_01 = wrap(inputs.range_behaviour.value, to_01);

    // - Shape
    to_01 = shape(inputs.shape_behaviour.value, to_01);

    // - Curve
    if(std::abs(inputs.curve.value) > float_min)
    {
      if(inputs.curve.value >= 0)
        to_01 = std::pow(std::abs(to_01) + float_min, std::pow(16., inputs.curve.value));
      else
        to_01
            = 1.
              - std::pow(
                  std::abs(1. - to_01) + float_min, std::pow(16., -inputs.curve.value));
    }

    // - Invert
    if(inputs.invert)
      to_01 = 1. - to_01;

    /// Unscale
    return to_01 * (inputs.out_max - inputs.out_min) + inputs.out_min;
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::absolute> a;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::control<&ins::midpoint> m;
        halp::control<&ins::deadzone> d;
      } midp;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::control<&ins::in_min> p;
        halp::control<&ins::min_learn> l;
      } min;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::control<&ins::in_max> p;
        halp::control<&ins::max_learn> l;
      } max;
    } in_range;

    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)

      halp::control<&ins::range_behaviour> rb;
      halp::control<&ins::shape_behaviour> sb;
      halp::control<&ins::invert> i;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::control<&ins::curve> c;
      } Knobs;
    } filter;

    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::out_min> min;
      halp::control<&ins::out_max> max;
    } out_range;
  };
};
}
