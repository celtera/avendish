#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/widgets.hpp>

#include <string>
#include <string_view>
#include <type_traits>

namespace avnd
{
/// Widget reflection ///

#define as_type(V) std::decay_t<decltype(V)>

struct bang
{
};
enum class widget_type
{
  bang,
  button,
  toggle,
  slider,
  spinbox,
  knob,
  iknob,
  lineedit,
  combobox,
  choices,
  xy,
  xy_spinbox,
  xyz,
  xyz_spinbox,
  color,
  time_chooser,
  bargraph,
  range_slider,
  range_spinbox
};

enum class slider_orientation
{
  horizontal,
  vertical
};

template <typename T>
struct widget_reflection
{
  using value_type = T;
  widget_type widget{widget_type::slider};
};

template <typename T>
struct slider_reflection
{
  using value_type = T;
  static constexpr widget_type widget = widget_type::slider;
  slider_orientation orientation{slider_orientation::horizontal};
};

template <typename T>
struct time_chooser_reflection
{
  using value_type = T;
  static constexpr widget_type widget = widget_type::time_chooser;
  slider_orientation orientation{slider_orientation::horizontal};
};

template <typename T>
struct range_slider_reflection
{
  using value_type = T;
  static constexpr widget_type widget = widget_type::range_slider;
  slider_orientation orientation{slider_orientation::horizontal};
};

template <typename T>
struct bargraph_reflection
{
  using value_type = T;
  static constexpr widget_type widget = widget_type::bargraph;
  slider_orientation orientation{slider_orientation::horizontal};
};

template <typename T>
consteval auto get_widget()
{
  using value_type = as_type(T::value);
  // Handle cases where an explicit widget is asked
  if constexpr(
      requires { T::widget::bang; } || requires { T::widget::impulse; })
  {
    return widget_reflection<bang>{widget_type::bang};
  }
  else if constexpr(
      requires { T::widget::button; } || requires { T::widget::pushbutton; })
  {
    return widget_reflection<bang>{widget_type::button};
  }
  else if constexpr(
      requires { T::widget::toggle; } || requires { T::widget::checkbox; })
  {
    return widget_reflection<bool>{widget_type::toggle};
  }
  else if constexpr(requires { T::widget::time_chooser; })
  {
    return time_chooser_reflection<float>{slider_orientation::horizontal};
  }
  else if constexpr(requires { T::widget::hslider; })
  {
    if constexpr(std::is_integral_v<value_type>)
    {
      return slider_reflection<int>{slider_orientation::horizontal};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return slider_reflection<float>{slider_orientation::horizontal};
    }
  }
  else if constexpr(requires { T::widget::vslider; })
  {
    if constexpr(std::is_integral_v<value_type>)
    {
      return slider_reflection<int>{slider_orientation::vertical};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return slider_reflection<float>{slider_orientation::vertical};
    }
  }
  else if constexpr(requires { T::widget::slider; })
  {
    if constexpr(std::is_integral_v<value_type>)
    {
      return slider_reflection<int>{slider_orientation::horizontal};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return slider_reflection<float>{slider_orientation::horizontal};
    }
  }
  else if constexpr(requires { T::widget::hrange_slider; })
  {
    using element_value_type = std::decay_t<decltype(value_type{}.start)>;
    if constexpr(std::is_integral_v<element_value_type>)
    {
      return range_slider_reflection<int>{slider_orientation::horizontal};
    }
    else if constexpr(std::is_floating_point_v<element_value_type>)
    {
      return range_slider_reflection<float>{slider_orientation::horizontal};
    }
  }
  else if constexpr(requires { T::widget::vrange_slider; })
  {
    using element_value_type = std::decay_t<decltype(value_type{}.start)>;
    if constexpr(std::is_integral_v<element_value_type>)
    {
      return range_slider_reflection<int>{slider_orientation::vertical};
    }
    else if constexpr(std::is_floating_point_v<element_value_type>)
    {
      return range_slider_reflection<float>{slider_orientation::vertical};
    }
  }
  else if constexpr(requires { T::widget::range_slider; })
  {
    using element_value_type = std::decay_t<decltype(value_type{}.start)>;
    if constexpr(std::is_integral_v<element_value_type>)
    {
      return range_slider_reflection<int>{slider_orientation::horizontal};
    }
    else if constexpr(std::is_floating_point_v<element_value_type>)
    {
      return range_slider_reflection<float>{slider_orientation::horizontal};
    }
  }
  else if constexpr(requires { T::widget::range_spinbox; })
  {
    using element_value_type = std::decay_t<decltype(value_type{}.start)>;
    if constexpr(std::is_integral_v<element_value_type>)
    {
      return widget_reflection<int>{widget_type::range_spinbox};
    }
    else if constexpr(std::is_floating_point_v<element_value_type>)
    {
      return widget_reflection<float>{widget_type::range_spinbox};
    }
  }
  else if constexpr(requires { T::widget::xyz_spinbox; })
  {
    return widget_reflection<float>{widget_type::xyz_spinbox};
  }
  else if constexpr(requires { T::widget::xy_spinbox; })
  {
    return widget_reflection<float>{widget_type::xy_spinbox};
  }
  else if constexpr(requires { T::widget::spinbox; })
  {
    if constexpr(requires { T::widget::xyz; })
    {
      return widget_reflection<float>{widget_type::xyz_spinbox};
    }
    else if constexpr(requires { T::widget::xy; })
    {
      return widget_reflection<float>{widget_type::xy_spinbox};
    }
    if constexpr(std::is_integral_v<value_type>)
    {
      return widget_reflection<int>{widget_type::spinbox};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return widget_reflection<float>{widget_type::spinbox};
    }
  }
  else if constexpr(requires { T::widget::knob; })
  {
    if constexpr(std::is_integral_v<value_type>)
    {
      return widget_reflection<int>{widget_type::knob};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return widget_reflection<float>{widget_type::knob};
    }
  }
  else if constexpr(requires { T::widget::iknob; })
  {
    return widget_reflection<int>{widget_type::knob};
  }
  else if constexpr(requires { T::widget::lineedit; })
  {
    return widget_reflection<value_type>{widget_type::lineedit};
  }
  else if constexpr(
      requires { T::widget::choices; } || requires { T::widget::enumeration; })
  {
    return widget_reflection<value_type>{widget_type::choices};
  }
  else if constexpr(
      requires { T::widget::combobox; } || requires { T::widget::list; })
  {
    return widget_reflection<value_type>{widget_type::combobox};
  }
  else if constexpr(requires { T::widget::xyz; })
  {
    return widget_reflection<value_type>{widget_type::xyz};
  }
  else if constexpr(requires { T::widget::xy; })
  {
    return widget_reflection<value_type>{widget_type::xy};
  }
  else if constexpr(requires { T::widget::color; })
  {
    return widget_reflection<value_type>{widget_type::color};
  }
  // Otherwise just try to make something that makes sense with the type
  else if constexpr(std::is_same_v<value_type, bool>)
  {
    return widget_reflection<bool>{widget_type::toggle};
  }
  else if constexpr(std::is_same_v<value_type, std::string>)
  {
    return widget_reflection<bool>{widget_type::lineedit};
  }

  // Handle outputs
  else if constexpr(requires { T::widget::hbargraph; })
  {
    if constexpr(std::is_integral_v<value_type>)
    {
      return bargraph_reflection<int>{slider_orientation::horizontal};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return bargraph_reflection<float>{slider_orientation::horizontal};
    }
  }
  else if constexpr(requires { T::widget::vbargraph; })
  {
    if constexpr(std::is_integral_v<value_type>)
    {
      return bargraph_reflection<int>{slider_orientation::vertical};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return bargraph_reflection<float>{slider_orientation::vertical};
    }
  }
  else if constexpr(requires { T::widget::bargraph; })
  {
    if constexpr(std::is_integral_v<value_type>)
    {
      return bargraph_reflection<int>{slider_orientation::horizontal};
    }
    else if constexpr(std::is_floating_point_v<value_type>)
    {
      return bargraph_reflection<float>{slider_orientation::horizontal};
    }
  }

  // Most general categories at the end
  else if constexpr(std::is_floating_point_v<value_type>)
  {
    return slider_reflection<value_type>{slider_orientation::horizontal};
  }
  else if constexpr(std::is_integral_v<value_type>)
  {
    return slider_reflection<value_type>{slider_orientation::horizontal};
  }
  else
  {
    return slider_reflection<value_type>{slider_orientation::horizontal};
  }
}

template <typename T>
consteval auto get_widget(const T&)
{
  return get_widget<T>();
}

}
