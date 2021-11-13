#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/input_introspection.hpp>
#include <fmt/format.h>

#include <QString>

namespace qml
{

struct enum_control
{
  template <typename Parent>
  void changed(Parent& parent, int idx, int value)
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

  template <typename Parent, avnd::enum_parameter C>
  void create(Parent& parent, C& c, int control_k)
  {
    std::string_view name = value_if_possible(C::name(), else, "Control");
    int init = static_cast<int>(value_if_possible(C::control().init, else, 0));

    // Concat enumerator texts
    std::string enumerators;
    enumerators.reserve(16 * C::choices().size());
    for (std::string_view e : C::choices())
    {
      enumerators += '"';
      enumerators += e;
      enumerators += "\", ";
    }

    // Remove last ,
    if (enumerators.size() > 0)
    {
      enumerators.pop_back();
      enumerators.pop_back();
    }

    // Add the control
    fmt::format_to(
        std::back_inserter(parent.componentData),
#include <avnd/binding/ui/qml/enum_ui.hpp>
        ,
        control_k,
        enumerators,
        init,
        name);
  }
};

}
