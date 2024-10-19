#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/export.hpp>
//#include <halp/callback.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <cmath>
#include <pybind11/chrono.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <span>
#include <string>

namespace python
{
inline const char* c_str(const std::string& v) noexcept
{
  return v.c_str();
}
inline consteval const char* c_str(std::string_view v) noexcept
{
  return v.data();
}
inline consteval const char* c_str(const char* v) noexcept
{
  return v;
}

template <std::size_t Sz>
inline const char* c_str(const std::array<char, Sz>& v) noexcept
{
  return v.data();
}

namespace py = pybind11;

template <auto Idx, typename C>
constexpr auto input_name(avnd::field_reflection<Idx, C>)
{
  if constexpr(avnd::has_name<C>)
    return avnd::get_name<C>();
  else
    return "input_" + std::to_string(Idx);
}

template <auto Idx, typename C>
constexpr auto output_name(avnd::field_reflection<Idx, C>)
{
  if constexpr(avnd::has_name<C>)
    return avnd::get_name<C>();
  else
    return "output_" + std::to_string(Idx);
}

template <typename T>
struct processor
{
  // m: pybind11 module
  py::class_<T> class_def;

  template <auto Idx, typename C>
  void setup_input(avnd::field_reflection<Idx, C> refl)
  {
    class_def.def_property(
        c_str(input_name(refl)),
        [](const T& t) { return avnd::pfr::get<Idx>(t.inputs).value; },
        [](T& t, decltype(C::value) x) { avnd::pfr::get<Idx>(t.inputs).value = x; });
  }

  template <auto Idx, typename C>
  void setup_output(avnd::field_reflection<Idx, C> refl)
  {
    if constexpr(requires { avnd::get_name<C>(); })
    {
      class_def.def_property(
          c_str(output_name(refl)),
          [](const T& t) { return avnd::pfr::get<Idx>(t.outputs).value; },
          [](T& t, decltype(C::value) x) { avnd::pfr::get<Idx>(t.outputs).value = x; });
    }
  }

  template <auto Idx, typename M>
  void setup_message(avnd::field_reflection<Idx, M>)
  {
    using func_type = decltype(avnd::message_get_func<M>());
    if constexpr(std::is_member_function_pointer_v<func_type>)
    {
      static constexpr auto func = avnd::message_get_func<M>();
      using refl = avnd::function_reflection<func>;
      using class_type = typename refl::class_type;
      if constexpr(std::is_same_v<class_type, M>)
      {
        auto synth_fun = []<typename... Args>(boost::mp11::mp_list<Args...>) {
          return [](Args... args) { M{}(args...); };
        };
        class_def.def(c_str(avnd::get_name<M>()), synth_fun(typename refl::arguments{}));
      }
      else
      {
        class_def.def(c_str(avnd::get_name<M>()), func);
      }
    }
    else if constexpr(requires { avnd::function_reflection<M::func()>::count; })
    {
      // TODO other cases: see pd
      class_def.def_static(c_str(avnd::get_name<M>()), avnd::message_get_func<M>());
    }
  }

  explicit processor(pybind11::module_& m)
      : class_def(m, c_str(avnd::get_c_identifier<T>()))
  {
    m.doc() = c_str(avnd::get_description<T>());

    class_def.def(py::init<>());
    if constexpr(requires { T{}(); })
    {
      class_def.def("process", &T::operator());
    }

    if constexpr(avnd::inputs_is_value<T>)
    {
      avnd::parameter_input_introspection<T>::for_all(
          [this](auto a) { setup_input(a); });
    }

    if constexpr(avnd::outputs_is_value<T>)
    {
      avnd::parameter_output_introspection<T>::for_all(
          [this](auto a) { setup_output(a); });
      avnd::callback_output_introspection<T>::for_all(
          [this](auto a) { setup_callback(a); });
    }

    avnd::messages_introspection<T>::for_all([this](auto a) { setup_message(a); });
  }

  template <std::size_t Idx, typename C>
  void setup_callback(const avnd::field_reflection<Idx, C>& out)
  {
    if constexpr(requires { avnd::get_name<C>(); })
    {
      using call_type = decltype(C::call);
      if constexpr(avnd::function_view_ish<call_type>)
      {
        /* not easily doable...
        class_def.def_property(
          c_str(C::name())
          , [] (T& t) { return avnd::pfr::get<Idx>(t.outputs).call; }
          , [] (T& t, call_type cb) { avnd::pfr::get<Idx>(t.outputs).call = std::move(cb); }
        );
        */
      }
      else if constexpr(avnd::function_ish<call_type>)
      {
        class_def.def_property(
            c_str(avnd::get_name<C>()),
            [](T& t) { return avnd::pfr::get<Idx>(t.outputs).call; },
            [](T& t, call_type cb) {
          avnd::pfr::get<Idx>(t.outputs).call = std::move(cb);
            });
      }
    }
  }
};
}
