#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/export.hpp>
//#include <halp/callback.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <boost/type_index.hpp>
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
  return avnd::get_static_symbol<C>();
}

template <auto Idx, typename C>
constexpr auto output_name(avnd::field_reflection<Idx, C>)
{
  return avnd::get_static_symbol<C>();
}

template <typename... Args>
struct register_types;
template <>
struct register_types<>
{
  void operator()(pybind11::module_& m) { }
};
template <typename Arg>
  requires std::is_aggregate_v<Arg>
struct register_types<Arg>
{
  void operator()(pybind11::module_& m)
  {
    static py::class_<Arg> class_def(
        m, boost::typeindex::type_id<Arg>().pretty_name().c_str()); // FIXME
    static auto reg = [] {
      Arg a;
      avnd::for_each_field_ref_n(
          a, []<typename M, std::size_t I>(const M&, avnd::field_index<I>) {
        static constexpr std::string_view nm = boost::pfr::get_name<I, Arg>();
        class_def.def_property(nm.data(), [](const Arg& a) {
          return avnd::pfr::get<I>(a);
        }, [](Arg& a, M x) { avnd::pfr::get<I>(a) = x; });
      });

      class_def.def("__repr__", [](const Arg& o) {
        std::string str = "{";

        avnd::for_each_field_ref_n(
            o, [&str]<typename M, std::size_t I>(const M& m, avnd::field_index<I>) {
          static constexpr std::string_view nm = boost::pfr::get_name<I, Arg>();
          str += nm;
          str += ": ";
          str += fmt::format("{}", m);
          str += ", ";
        });
        str += "}";
        return str;
      });
      return 0;
    }();
  }
};

template <typename Arg, typename... Args>
struct register_types<Arg, Args...>
{
  void operator()(pybind11::module_& m)
  {
    register_types<Arg>{}(m);
    register_types<Args...>{}(m);
  }
};

template <typename, typename>
struct register_arg_types;
template <typename stdfunc_type, typename R, typename... Args>
struct register_arg_types<stdfunc_type, R(Args...)>
{
  auto operator()(pybind11::module_& m) { register_types<Args...>{}(m); }
};

template <typename, typename>
struct make_lambda_for_args;
template <typename stdfunc_type, typename R, typename... Args>
struct make_lambda_for_args<stdfunc_type, R(Args...)>
{
  auto operator()()
  {
    return +[](void* ctx, Args... args) {
      return (*(stdfunc_type*)(ctx))(std::forward<Args>(args)...);
    };
  }
};

template <typename T>
struct processor
{
  // m: pybind11 module
  py::class_<T> class_def;

  template <auto Idx, typename C>
  void setup_input(avnd::field_reflection<Idx, C> refl)
  {
    class_def.def_property(c_str(input_name(refl)), [](const T& t) {
      return avnd::pfr::get<Idx>(t.inputs).value;
    }, [](T& t, decltype(C::value) x) {
      avnd::pfr::get<Idx>(t.inputs).value = x;
      auto& inputs = avnd::get_inputs(t);
      auto& field = boost::pfr::get<Idx>(inputs);
      if_possible(field.update(t));
    });
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
        class_def.def(
            c_str(avnd::get_static_symbol<M>()), synth_fun(typename refl::arguments{}));
      }
      else
      {
        class_def.def(c_str(avnd::get_static_symbol<M>()), func);
      }
    }
    else if constexpr(requires { avnd::function_reflection<M::func()>::count; })
    {
      // TODO other cases: see pd
      class_def.def_static(
          c_str(avnd::get_static_symbol<M>()), avnd::message_get_func<M>());
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
          [this, &m](auto a) { setup_callback(a, m); });
    }

    avnd::messages_introspection<T>::for_all([this](auto a) { setup_message(a); });
  }

  template <std::size_t Idx, typename C>
  void setup_callback(const avnd::field_reflection<Idx, C>& out, pybind11::module_& m)
  {
    if constexpr(requires { avnd::get_static_symbol<C>(); })
    {
      using call_type = decltype(C::call);

      if constexpr(avnd::function_view_ish<call_type>)
      {
        using func_type = typename call_type::type;
        using stdfunc_type = std::function<func_type>;
        register_arg_types<stdfunc_type, func_type>{}(m);

        class_def.def_property(c_str(avnd::get_static_symbol<C>()), [](T& t) {
          return avnd::pfr::get<Idx>(t.outputs).call;
        }, [](T& t, stdfunc_type cb) {
          auto& cur = avnd::pfr::get<Idx>(t.outputs).call;
          if(cur.context)
            delete(stdfunc_type*)cur.context;

          cur.context = new stdfunc_type{std::move(cb)};
          cur.function = make_lambda_for_args<stdfunc_type, func_type>{}();
        });
      }
      else if constexpr(avnd::function_ish<call_type>)
      {
        class_def.def_property(c_str(avnd::get_static_symbol<C>()), [](T& t) {
          return avnd::pfr::get<Idx>(t.outputs).call;
        }, [](T& t, call_type cb) {
          avnd::pfr::get<Idx>(t.outputs).call = std::move(cb);
        });
      }
      else
      {
        static_assert(C::callback_type_unsupported);
      }
    }
  }
};
}
