#pragma once

// This example leverages the ossia easing segment library
// ; feel free to just copy the file as it is pretty standalone
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>

namespace ao
{
/**
 * @brief Tween from a value to another over time
 */
struct Tweener
{
public:
  halp_meta(name, "Tweener")
  halp_meta(c_name, "tweener")
  halp_meta(category, "Automations")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Interpolate between two values over a time span")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/tweener.html")
  halp_meta(uuid, "1aea36d3-b039-4800-a488-9f3d8a0865a5")

  struct inputs_t
  {
    struct from_t : halp::val_port<"From...", float>
    {
      static constexpr bool event = false;
    } from;
    struct to_t : halp::val_port<"To...", float>
    {
      static constexpr bool event = false;
    } to;
    struct
    {
      halp__enum_combobox(
          "Ease", Linear, Linear, BackIn, BackOut, BackInOut, BounceIn, BounceOut,
          BounceInOut, CircularIn, CircularOut, CircularInOut, CubicIn, CubicOut,
          CubicInOut, ExponentialIn, ExponentialOut, ExponentialInOut, ElasticIn,
          ElasticOut, ElasticInOut, PerlinInOut, QuadraticIn, QuadraticOut,
          QuadraticInOut, QuarticIn, QuarticOut, QuarticInOut, QuinticIn, QuinticOut,
          QuinticInOut, SineIn, SineOut, SineInOut);
    } ease{};
  } inputs;

  struct
  {
    halp::val_port<"Output", float> output;
  } outputs;

  struct tick
  {
    double relative_position{};
  };

  double ease01(double value)
  {
    using ease_type = decltype(inputs.ease)::enum_type;
    switch(inputs.ease)
    {
      default:
      case ease_type::Linear:
        return ossia::easing::linear{}(value);
      case ease_type::BackIn:
        return ossia::easing::backIn{}(value);
      case ease_type::BackOut:
        return ossia::easing::backOut{}(value);
      case ease_type::BackInOut:
        return ossia::easing::backInOut{}(value);
      case ease_type::BounceIn:
        return ossia::easing::bounceIn{}(value);
      case ease_type::BounceOut:
        return ossia::easing::bounceOut{}(value);
      case ease_type::BounceInOut:
        return ossia::easing::bounceInOut{}(value);
      case ease_type::QuadraticIn:
        return ossia::easing::quadraticIn{}(value);
      case ease_type::QuadraticOut:
        return ossia::easing::quadraticOut{}(value);
      case ease_type::QuadraticInOut:
        return ossia::easing::quadraticInOut{}(value);
      case ease_type::CubicIn:
        return ossia::easing::cubicIn{}(value);
      case ease_type::CubicOut:
        return ossia::easing::cubicOut{}(value);
      case ease_type::CubicInOut:
        return ossia::easing::cubicInOut{}(value);
      case ease_type::QuarticIn:
        return ossia::easing::quarticIn{}(value);
      case ease_type::QuarticOut:
        return ossia::easing::quarticOut{}(value);
      case ease_type::QuarticInOut:
        return ossia::easing::quarticInOut{}(value);
      case ease_type::QuinticIn:
        return ossia::easing::quinticIn{}(value);
      case ease_type::QuinticOut:
        return ossia::easing::quinticOut{}(value);
      case ease_type::QuinticInOut:
        return ossia::easing::quinticInOut{}(value);
      case ease_type::SineIn:
        return ossia::easing::sineIn{}(value);
      case ease_type::SineOut:
        return ossia::easing::sineOut{}(value);
      case ease_type::SineInOut:
        return ossia::easing::sineInOut{}(value);
      case ease_type::CircularIn:
        return ossia::easing::circularIn{}(value);
      case ease_type::CircularOut:
        return ossia::easing::circularOut{}(value);
      case ease_type::CircularInOut:
        return ossia::easing::circularInOut{}(value);
      case ease_type::ExponentialIn:
        return ossia::easing::exponentialIn{}(value);
      case ease_type::ExponentialOut:
        return ossia::easing::exponentialOut{}(value);
      case ease_type::ExponentialInOut:
        return ossia::easing::exponentialInOut{}(value);
      case ease_type::ElasticIn:
        return ossia::easing::elasticIn{}(value);
      case ease_type::ElasticOut:
        return ossia::easing::elasticOut{}(value);
      case ease_type::ElasticInOut:
        return ossia::easing::elasticInOut{}(value);
      case ease_type::PerlinInOut:
        return ossia::easing::perlinInOut{}(value);
    }
  }

  void operator()(tick t) noexcept
  {
    double curve_ease = ease01(std::clamp(t.relative_position, 0., 1.));

    outputs.output
        = ossia::easing::ease{}(inputs.from.value, inputs.to.value, curve_ease);
    if(!std::isfinite(outputs.output) || !std::isnormal(outputs.output))
    {
      std::abort();
    }
  }
};

}
