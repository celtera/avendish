#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/messages_introspection.hpp>
#include <cmath>
#include <avnd/common/export.hpp>
#include <avnd/helpers/callback.hpp>
#include <pybind11/chrono.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <span>
#include <string>

namespace python
{
inline consteval const char* c_str(std::string v) noexcept = delete;
inline consteval const char* c_str(std::string_view v) noexcept
{
  return v.data();
}
inline consteval const char* c_str(const char* v) noexcept
{
  return v;
}

namespace py = pybind11;

template <typename T>
struct processor
{
  // m: pybind11 module
  py::class_<T> class_def;

  template <auto Idx, typename C>
  void setup_input(avnd::field_reflection<Idx, C>)
  {
    if constexpr (requires { C::name(); })
    {
      class_def.def_property(
          c_str(C::name()),
          [](const T& t) { return boost::pfr::get<Idx>(t.inputs).value; },
          [](T& t, decltype(C::value) x)
          { boost::pfr::get<Idx>(t.inputs).value = x; });
    }
  }

  template <auto Idx, typename C>
  void setup_output(avnd::field_reflection<Idx, C>)
  {
    if constexpr (requires { C::name(); })
    {
      class_def.def_property(
          c_str(C::name()),
          [](const T& t) { return boost::pfr::get<Idx>(t.outputs).value; },
          [](T& t, decltype(C::value) x)
          { boost::pfr::get<Idx>(t.outputs).value = x; });
    }
  }

  template <auto Idx, typename M>
  void setup_message(avnd::field_reflection<Idx, M>)
  {
    if constexpr (std::is_member_function_pointer_v<decltype(M::func())>)
    {
      constexpr auto mem_fun = M::func();
      class_def.def(c_str(M::name()), mem_fun);
    }
    else if constexpr (requires
                       { avnd::function_reflection<M::func()>::count; })
    {
      // TODO other cases: see pd
      class_def.def_static(c_str(M::name()), M::func());
    }
  }

  explicit processor(pybind11::module_& m)
      : class_def(m, "processor")
  {
    m.doc() = c_str(T::name());

    class_def.def(py::init<>());
    if constexpr (requires { T{}(); })
    {
      class_def.def("process", &T::operator());
    }

    if constexpr (avnd::inputs_is_value<T>)
    {
      avnd::parameter_input_introspection<T>::for_all([this](auto a)
                                                      { setup_input(a); });
    }

    if constexpr (avnd::outputs_is_value<T>)
    {
      avnd::parameter_output_introspection<T>::for_all([this](auto a)
                                                       { setup_output(a); });
      avnd::callback_output_introspection<T>::for_all([this](auto a)
                                                      { setup_callback(a); });
    }

    avnd::messages_introspection<T>::for_all([this](auto a)
                                             { setup_message(a); });
  }

  template <std::size_t Idx, typename C>
  void setup_callback(const avnd::field_reflection<Idx, C>& out)
  {
    if constexpr (requires { C::name(); })
    {
      using call_type = decltype(C::call);
      if constexpr (avnd::function_view_ish<call_type>)
      {
        /* not easily doable...
        class_def.def_property(
          c_str(C::name())
          , [] (T& t) { return boost::pfr::get<Idx>(t.outputs).call; }
          , [] (T& t, call_type cb) { boost::pfr::get<Idx>(t.outputs).call = std::move(cb); }
        );
        */
      }
      else if constexpr (avnd::function_ish<call_type>)
      {
        class_def.def_property(
            c_str(C::name()),
            [](T& t) { return boost::pfr::get<Idx>(t.outputs).call; },
            [](T& t, call_type cb)
            { boost::pfr::get<Idx>(t.outputs).call = std::move(cb); });
      }
    }
  }
};
}
