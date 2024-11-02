#pragma once
#include <halp/controls.hpp>
#include <halp/modules.hpp>
#include <halp/smoothers.hpp>

HALP_MODULE_EXPORT
namespace halp
{
template <typename T>
struct smooth_control : T
{
  using smooth = halp::milliseconds_smooth<15>;
};

template <static_string lit, auto setup = default_range<float>>
struct smooth_knob : halp::knob_t<float, lit, setup>
{
  using smooth = halp::milliseconds_smooth<15>;
};

template <static_string lit, auto setup = default_range<float>>
struct smooth_slider : halp::slider_t<float, lit, setup>
{
  using smooth = halp::milliseconds_smooth<15>;
};

}
