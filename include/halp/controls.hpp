#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/predef.h>
#include <halp/inline.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include <halp/controls.basic.hpp>
#include <halp/controls.buttons.hpp>
#include <halp/controls.enums.hpp>

#if(!BOOST_COMP_GNUC || (BOOST_COMP_GNUC >= 12)) && !defined(ESP8266)
#include <halp/controls.sliders.hpp>
#else
#define HALP_GCC10_SLIDERS_WORKAROUND 1
#include <halp/controls.sliders.gcc10.hpp>
#endif

namespace halp
{

/// Basic widgets ///
/// Button ///

/// LineEdit ///
struct lineedit_setup
{
  std::string_view init;
};

template <static_string lit, static_string setup>
struct lineedit_t
{
  lineedit_t() noexcept
      : value{setup.value}
  {
  }

  enum widget
  {
    lineedit,
    textedit,
    text
  };
  struct range
  {
    const std::string_view init = setup.value;
  };

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  std::string value = setup.value;
  operator std::string&() noexcept { return value; }
  operator const std::string&() const noexcept { return value; }
  auto& operator=(std::string&& t) noexcept
  {
    value = std::move(t);
    return *this;
  }
  auto& operator=(const std::string& t) noexcept
  {
    value = t;
    return *this;
  }
  auto& operator=(std::string_view t) noexcept
  {
    value = t;
    return *this;
  }
};

// Useful typedefs
/// Bargraph ///
template <typename T, static_string lit, auto setup>
struct hbargraph_t : slider_t<T, lit, setup>
{
  enum widget
  {
    hbargraph,
    bargraph
  };
  auto& operator=(T t) noexcept
  {
    this->value = t;
    return *this;
  }
};

template <typename T, static_string lit, auto setup>
struct vbargraph_t : slider_t<T, lit, setup>
{
  enum widget
  {
    vbargraph,
    bargraph
  };
  auto& operator=(T t) noexcept
  {
    this->value = t;
    return *this;
  }
};

/// Slider ///
template <typename T, static_string lit, auto setup>
struct hslider_t : slider_t<T, lit, setup>
{
  enum widget
  {
    hslider,
    slider
  };
};

template <typename T, static_string lit, auto setup>
struct vslider_t : slider_t<T, lit, setup>
{
  enum widget
  {
    vslider,
    slider
  };
};

/// Spinbox ///
template <typename T, static_string lit, auto setup>
struct spinbox_t : slider_t<T, lit, setup>
{
  enum widget
  {
    spinbox
  };
};

/// Knob ///
template <typename T, static_string lit, auto setup>
struct knob_t : slider_t<T, lit, setup>
{
  enum widget
  {
    knob
  };
};

// For the case where we want a "float value;" but an integer widget
template <typename T, static_string lit, auto setup>
struct iknob_t : slider_t<T, lit, setup>
{
  enum widget
  {
    iknob
  };
};

/// Toggle ///
template <typename T, static_string lit, auto setup>
struct toggle_t : slider_t<T, lit, setup>
{
  enum widget
  {
    toggle,
    checkbox
  };
};

template <static_string lit, auto setup = default_range<float>>
using hslider_f32 = halp::hslider_t<float, lit, setup>;
template <static_string lit, auto setup = default_range<int>>
using hslider_i32 = halp::hslider_t<int, lit, setup>;

template <static_string lit, auto setup = default_range<float>>
using vslider_f32 = halp::vslider_t<float, lit, setup>;
template <static_string lit, auto setup = default_range<int>>
using vslider_i32 = halp::vslider_t<int, lit, setup>;
template <static_string lit, auto setup = default_range<float>>
using spinbox_f32 = halp::spinbox_t<float, lit, setup>;
template <static_string lit, auto setup = default_range<int>>
using spinbox_i32 = halp::spinbox_t<int, lit, setup>;

template <static_string lit>
using impulse_button = impulse_button_t<lit>;

template <static_string lit, static_string setup>
using lineedit = lineedit_t<lit, setup>;

template <static_string lit, auto setup = default_range<float>>
using knob_f32 = halp::knob_t<float, lit, setup>;
template <static_string lit, auto setup = default_irange<int>>
using knob_i32 = halp::knob_t<int, lit, setup>;

template <static_string lit, auto setup = default_irange<int>>
using iknob_f32 = halp::knob_t<float, lit, setup>;

// template <static_string lit, long double min, long double max, long double init>
// using knob = halp::knob_t<float, lit, halp::range{min, max, init}>;

// Necessary because we have that "toggle" enum member..
template <static_string lit, toggle_setup setup = default_toggle>
using toggle = toggle_t<bool, lit, setup>;

template <static_string lit, toggle_setup setup = default_toggle>
using toggle_f32 = toggle_t<float, lit, setup.to_range()>;

template <static_string lit>
using maintained_button = maintained_button_t<lit>;

#if !HALP_GCC10_SLIDERS_WORKAROUND
template <static_string lit, range setup = range{.min = 0.001, .max = 5., .init = 0.25}>
using time_chooser = time_chooser_t<float, lit, setup>;

template <static_string lit, range_slider_range setup = range_slider_range{}>
using range_slider_f32 = halp::range_slider_t<float, lit, setup>;
#else

template <static_string lit, range setup = range{}>
using time_chooser = time_chooser_t<float, lit, setup>;

template <static_string lit, range_slider_range setup = default_range<float>>
using range_slider_f32 = halp::range_slider_t<float, lit, setup>;
#endif

template <static_string lit, range setup = default_range<float>>
using xy_pad_f32 = halp::xy_pad_t<float, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using xy_spinboxes_f32 = halp::xy_spinboxes_t<float, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using xyz_spinboxes_f32 = halp::xyz_spinboxes_t<float, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using hbargraph_f32 = halp::hbargraph_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using hbargraph_i32 = halp::hbargraph_t<int, lit, setup>;

template <static_string lit, range setup = default_range<float>>
using vbargraph_f32 = halp::vbargraph_t<float, lit, setup>;
template <static_string lit, range setup = default_range<int>>
using vbargraph_i32 = halp::vbargraph_t<int, lit, setup>;

}

#define halp_field_names(...)                                                    \
  static constexpr auto field_names()                                            \
  {                                                                              \
    constexpr auto r = std::array<std::string_view, HALP_NUM_ARGS(__VA_ARGS__)>{ \
        HALP_STRING_LITERAL_ARRAY(__VA_ARGS__)};                                 \
    return r;                                                                    \
  }
