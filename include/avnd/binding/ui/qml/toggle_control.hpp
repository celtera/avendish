#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <fmt/format.h>

#include <QString>

namespace qml
{

struct toggle_control
{
  template <typename Parent>
  void changed(Parent& parent, int idx, bool value)
  {
    using T = typename Parent::type;
    avnd::parameter_input_introspection<T>::for_nth_raw(
        avnd::get_inputs(parent.implementation),
        idx,
        [value]<typename C>(C& ctl)
        {
          if constexpr (avnd::enum_parameter<C>)
            ctl.value = static_cast<decltype(C::value)>(value);
        });
  }

  template <typename Parent, avnd::bool_parameter C>
  void create(Parent& parent, C& c, int control_k)
  {
    std::string_view name = value_if_possible(C::name(), else, "Control");
    bool init = static_cast<bool>(value_if_possible(avnd::get_range<C>().init, else, 0));

    // Add the control
    fmt::format_to(
        std::back_inserter(parent.componentData),
#include <avnd/binding/ui/qml/toggle_ui.hpp>
        ,
        control_k,
        init ? "true" : "false",
        name);
  }
};

}
