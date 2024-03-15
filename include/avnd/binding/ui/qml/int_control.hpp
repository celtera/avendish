/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once
#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <fmt/format.h>

#include <QString>

namespace qml
{

struct int_control
{
  template <typename Parent>
  void changed(Parent& parent, int idx, int value)
  {
    using T = typename Parent::type;
    avnd::parameter_input_introspection<T>::for_nth_raw(
        avnd::get_inputs(parent.implementation), idx,
        [&parent, value]<typename C>(C& ctl) {
      if constexpr(avnd::int_parameter<C>)
      {
        ctl.value = value;
        if_possible(ctl.update(parent.implementation));
      }
        });
  }

  template <typename Parent>
  QString display(Parent& parent, int idx, int value)
  {
    return parent.value_display(idx, value);
  }

  template <typename Parent, avnd::int_parameter C>
    requires(!avnd::enum_ish_parameter<C>)
  void create(Parent& parent, C& c, int control_k)
  {
    std::string_view name = value_if_possible(C::name(), else, "Control");
    int min = value_if_possible(avnd::get_range<C>().min, else, 0);
    int max = value_if_possible(avnd::get_range<C>().max, else, 127);
    int init = value_if_possible(avnd::get_range<C>().init, else, 64);
    int step = value_if_possible(avnd::get_range<C>().step, else, 1);

    if constexpr(requires { C::widget::knob; })
    {
      fmt::format_to(
          std::back_inserter(parent.componentData),
#include <avnd/binding/ui/qml/int_knob.hpp>
          , control_k, min, max, init, step, name);
    }
    else
    {
      fmt::format_to(
          std::back_inserter(parent.componentData),
#include <avnd/binding/ui/qml/int_slider.hpp>
          , control_k, min, max, init, step, name);
    }
  }
};

}
