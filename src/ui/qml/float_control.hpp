#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/effect_container.hpp>
#include <avnd/input_introspection.hpp>
#include <fmt/format.h>

#include <QString>

namespace qml
{

struct float_control
{

  template <typename Parent>
  void changed(Parent& parent, int idx, float value)
  {
    using T = typename Parent::type;
    avnd::parameter_input_introspection<T>::for_nth(
        avnd::get_inputs(parent.implementation),
        idx,
        [value]<typename C>(C& ctl)
        {
          if constexpr (avnd::float_parameter<C>)
            ctl.value = value;
        });
  }

  template <typename Parent>
  QString display(Parent& parent, int idx, float value)
  {
    return parent.value_display(idx, value);
  }

  template <typename Parent, avnd::float_parameter C>
  void create(Parent& parent, C& c, int control_k)
  {
    std::string_view name = value_if_possible(C::name(), else, "Control");
    float min = value_if_possible(C::control().min, else, 0.f);
    float max = value_if_possible(C::control().max, else, 1.f);
    float init = value_if_possible(C::control().init, else, 0.5f);
    float step = value_if_possible(C::control().step, else, 0.0f);

    if constexpr (requires { C::widget::knob; })
    {
      fmt::format_to(
          std::back_inserter(parent.componentData),
#include <ui/qml/float_knob.hpp>
          ,
          control_k,
          min,
          max,
          init,
          step,
          name);
    }
    else
    {
      fmt::format_to(
          std::back_inserter(parent.componentData),
#include <ui/qml/float_slider.hpp>
          ,
          control_k,
          min,
          max,
          init,
          step,
          name);
    }
  }
};

}
