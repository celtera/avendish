#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/export.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <lo/lo.h>

#include <cmath>
#include <iostream>
#include <algorithm>
#include <span>
#include <string>

namespace osc
{
using arg_iterator = avnd::generator<std::variant<std::monostate, int, float, bool, std::string_view>>;

inline arg_iterator make_arg_iterator(std::string_view types, std::span<lo_arg*> args)
{
  int k = 0;
  for(lo_arg* arg: args) {
    switch(types[k])
    {
      case LO_INT32:
        co_yield arg->i32;
        break;
      case LO_INT64:
        co_yield (int)arg->i64;
        break;
      case LO_FLOAT:
        co_yield arg->f32;
        break;
      case LO_DOUBLE:
        co_yield (float)arg->f64;
        break;
      case LO_CHAR:
        co_yield arg->c;
        break;
      case LO_MIDI:
        co_yield std::monostate{};
        break;
      case LO_STRING:
        co_yield arg->s;
        break;
      case LO_SYMBOL:
        co_yield arg->S;
        break;
      case LO_BLOB:
        co_yield std::monostate{};
        break;
      case LO_TIMETAG:
        co_yield std::monostate{};
        break;
      case LO_TRUE:
        co_yield true;
        break;
      case LO_FALSE:
        co_yield false;
        break;
      case LO_NIL:
        co_yield std::monostate{};
        break;
      case LO_INFINITUM:
        co_yield std::monostate{};
        break;
    }

    k++;
  }
}

template<typename T>
inline avnd::generator<T> make_arg_iterator_t(std::string_view types, std::span<lo_arg*> args)
{
#define try_yield(element) \
  if constexpr(std::is_constructible_v<T, decltype(arg->element)>) \
    co_yield arg->element; \
  else \
    co_yield T{};

  int k = 0;
  for(lo_arg* arg: args) {
    switch(types[k])
    {
      case LO_INT32: try_yield(i32); break;
      case LO_INT64: try_yield(i64); break;
      case LO_FLOAT: try_yield(f32); break;
      case LO_DOUBLE: try_yield(f64); break;
      case LO_CHAR: try_yield(c); break;
      case LO_STRING: try_yield(s); break;
      case LO_SYMBOL: try_yield(S); break;
      case LO_TRUE:
      case LO_FALSE:
        if constexpr(std::is_constructible_v<T, bool>)
          co_yield (types[k] == LO_TRUE);
        else
          co_yield T{};
        break;

      case LO_MIDI:
      case LO_BLOB:
      case LO_TIMETAG:
      case LO_NIL:
      case LO_INFINITUM:
      default:
        co_yield T{};
        break;
    }

    k++;

  }
#undef try_yield
}

template <typename T>
struct liblo_recv_binding
{
  liblo_recv_binding(std::shared_ptr<T> obj, std::string_view port, std::string_view root = "")
    : object{std::move(obj)}
    , root{root}
  {
    constexpr auto error_handler = [] (int num, const char *msg, const char *path) {
      std::cerr << "liblo: error: " << num << " in " << path << ": " << msg << '\n';
    };

    osc_server = lo_server_thread_new(port.data(), error_handler);

    constexpr auto message_handler = [] (const char *p , const char *types, lo_arg ** argv,
        int argc, lo_message data, void *user_data) -> int {
        return static_cast<liblo_recv_binding*>(user_data)->handler(p, types, std::span<lo_arg*>(argv, argc));
    };

    lo_server_thread_add_method(osc_server, nullptr, nullptr, message_handler, this);
    lo_server_thread_start(osc_server);
  }

  ~liblo_recv_binding()
  {
    lo_server_thread_stop(osc_server);
    lo_server_thread_free(osc_server);
  }

  template<typename C>
  static bool is_path(const C& f, std::string_view path) noexcept
  {
    if constexpr(requires { C::osc_address(); })
    {
      return path == C::osc_address();
    }
    else
    {
      if(!path.starts_with('/'))
        return false;
      return path.substr(1) == avnd::get_name<C>();
    }
  }

  template<typename V>
  requires ((avnd::int_ish<V> || avnd::fp_ish<V>) && (!avnd::enum_ish<V>))
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    if(types[0] == LO_INT32)
      avnd::apply_control(field, args[0]->i32);
    else if(types[0] == LO_INT64)
      avnd::apply_control(field, args[0]->i64);
    else if(types[0] == LO_FLOAT)
      avnd::apply_control(field, args[0]->f32);
    else if(types[0] == LO_DOUBLE)
      avnd::apply_control(field, args[0]->f64);
    else if(types[0] == LO_TRUE)
      avnd::apply_control(field, 1);
    else if(types[0] == LO_FALSE)
      avnd::apply_control(field, 0);
  }

  template<avnd::parameter_with_values_range F, avnd::enum_ish V>
  void apply(F& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    if(types[0] == LO_STRING)
      avnd::apply_control(field, &args[0]->s);
    else if(types[0] == LO_SYMBOL)
      avnd::apply_control(field, &args[0]->S);
  }

  void apply(auto& field, std::string& value, std::string_view types, std::span<lo_arg*> args)
  {
    if(types[0] == LO_STRING)
      avnd::apply_control(field, &args[0]->s);
    else if(types[0] == LO_SYMBOL)
      avnd::apply_control(field, &args[0]->S);
  }

  void apply(auto& field, bool& value, std::string_view types, std::span<lo_arg*> args)
  {
    if(types[0] == LO_TRUE)
      avnd::apply_control(field, true);
    else if(types[0] == LO_FALSE)
      avnd::apply_control(field, false);
  }

  template<std::size_t N, typename V>
  void apply(auto& field, std::array<V, N>& value, std::string_view types, std::span<lo_arg*> args)
  {
    int i = 0;
    for(auto& element : make_arg_iterator_t<V>(types, args)) {
      if(i >= N)
        return;

      value[i] = element;
    }
  }

  template<avnd::vector_ish V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    value.clear();
    for(auto& element : make_arg_iterator_t<typename V::value_type>(types, args)) {
      value.push_back(element);
    }

    // TODO apply range
  }

  template<avnd::bitset_ish V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    // TODO
  }

  template<avnd::set_ish V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    value.clear();
    for(auto& element : make_arg_iterator_t<typename V::value_type>(types, args)) {
      value.insert(element);
    }

    // TODO apply range
  }

  template<avnd::map_ish V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    // TODO
  }

  template<avnd::optional_ish V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    value.reset();
    for(auto& element : make_arg_iterator_t<typename V::value_type>(types, args)) {
      value = element;
      break;
    }

    // TODO apply range
  }

  template<avnd::tuple_ish V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    // TODO
  }

  template<avnd::variant_ish V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    // TODO
  }

  // Generic aggregate case
  template<typename V>
  void apply(auto& field, V& value, std::string_view types, std::span<lo_arg*> args)
  {
    // TODO
  }

  void dispatch(std::string_view path, std::string_view types, std::span<lo_arg*> args)
  {
    if(types.empty())
      return;

    avnd::input_introspection<T>::for_all(
          avnd::get_inputs(*object),
          [=] <typename F> (F& field) {
       if(is_path(field, path))
       {
         apply(field, field.value, types, args);
         return true;
       }
       return false;
    });
  }

  int handler(std::string_view path, std::string_view types, std::span<lo_arg*> args)
  {
    if(!path.starts_with(root))
      return 1;
    auto sub = path.substr(root.length());
    dispatch(sub, types, args);

    return 1;
  }

private:
  std::shared_ptr<T> object;
  lo_server_thread osc_server;
  std::string_view root;
};
}
