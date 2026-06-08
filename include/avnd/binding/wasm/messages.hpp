#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Message dispatch for the WASM backend. Mirrors pd/messages.hpp but reads
 * arguments from emscripten::val instead of t_atom. Wrong argument counts
 * return false and are never invoked.
 */

#include <avnd/binding/wasm/controls.hpp>
#include <avnd/concepts/message.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <boost/mp11.hpp>

#include <string>
#include <string_view>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace wasm
{

#if defined(__EMSCRIPTEN__)

// Owns the converted value with stable lifetime; .get() yields it in the form
// the target function expects. const char*/string_view args are backed by an
// owned std::string (from_js can't materialise a raw char buffer).
template <typename Arg>
struct msg_arg_holder
{
  std::decay_t<Arg> value{};
  explicit msg_arg_holder(const emscripten::val& v) { wasm::from_js(value, v); }
  std::decay_t<Arg>& get() { return value; }
};

template <typename Arg>
  requires(std::is_same_v<std::decay_t<Arg>, const char*>
           || std::is_same_v<std::decay_t<Arg>, char*>)
struct msg_arg_holder<Arg>
{
  std::string storage{};
  explicit msg_arg_holder(const emscripten::val& v)
  {
    if(v.isString())
      storage = v.as<std::string>();
  }
  const char* get() { return storage.c_str(); }
};

template <typename Arg>
inline msg_arg_holder<Arg> make_msg_arg(const emscripten::val& v)
{
  return msg_arg_holder<Arg>{v};
}

// Call a message that does NOT take the instance as its first argument.
template <typename M, typename T>
inline bool call_message_static(T& implementation, const emscripten::val& args)
{
  using refl = avnd::message_reflection<M>;
  static constexpr auto arg_count = refl::count;

  const int argc = js_is_array(args) ? args["length"].as<int>() : 0;
  if((int)arg_count != argc)
    return false;

  using arg_list_t = typename refl::arguments;

  [&]<typename... Args, std::size_t... I>(
      boost::mp11::mp_list<Args...>, std::index_sequence<I...>) {
    static constexpr auto f = avnd::message_get_func<M>();
    auto holders = std::tuple{make_msg_arg<Args>(args[(int)I])...};
    if constexpr(std::is_member_function_pointer_v<std::decay_t<decltype(f)>>)
    {
      if constexpr(requires(M m) { m(std::get<I>(holders).get()...); })
        M{}(std::get<I>(holders).get()...);
      else if constexpr(requires {
                          (implementation.*f)(std::get<I>(holders).get()...);
                        })
        (implementation.*f)(std::get<I>(holders).get()...);
    }
    else
    {
      f(std::get<I>(holders).get()...);
    }
  }(arg_list_t{}, std::make_index_sequence<arg_count>());

  return true;
}

// Call a message whose first reflected argument is `T&` (the instance): JS
// supplies (arg_count - 1) values and we splice `implementation` in front.
template <typename M, typename T>
inline bool call_message_instance(T& implementation, const emscripten::val& args)
{
  using refl = avnd::message_reflection<M>;
  static constexpr auto arg_count = refl::count;

  const int argc = js_is_array(args) ? args["length"].as<int>() : 0;
  if((int)arg_count != argc + 1)
    return false;

  using arg_list_t = typename refl::arguments;

  [&]<typename Self, typename... Args, std::size_t... I>(
      boost::mp11::mp_list<Self, Args...>, std::index_sequence<I...>) {
    static constexpr auto f = avnd::message_get_func<M>();
    auto holders = std::tuple{make_msg_arg<Args>(args[(int)I])...};
    if constexpr(std::is_member_function_pointer_v<std::decay_t<decltype(f)>>)
    {
      if constexpr(requires(M m) {
                     m(implementation, std::get<I>(holders).get()...);
                   })
        M{}(implementation, std::get<I>(holders).get()...);
      else if constexpr(requires {
                          (implementation.*f)(
                              implementation, std::get<I>(holders).get()...);
                        })
        (implementation.*f)(implementation, std::get<I>(holders).get()...);
    }
    else
    {
      f(implementation, std::get<I>(holders).get()...);
    }
  }(arg_list_t{}, std::make_index_sequence<arg_count - 1>());

  return true;
}

// Dispatch a single reflected message field.
template <typename M, typename T>
inline bool invoke_message(T& implementation, const emscripten::val& args)
{
  if constexpr(!std::is_void_v<avnd::message_reflection<M>>)
  {
    static constexpr auto arg_count = avnd::message_reflection<M>::count;
    if constexpr(arg_count == 0)
    {
      return call_message_static<M>(implementation, args);
    }
    else if constexpr(std::is_same_v<avnd::first_message_argument<M>, T&>)
    {
      return call_message_instance<M>(implementation, args);
    }
    else
    {
      return call_message_static<M>(implementation, args);
    }
  }
  return false;
}

// Find the message named `name` and invoke it; true if found and invoked.
template <typename T>
inline bool dispatch_message(
    avnd::effect_container<T>& obj, std::string_view name, const emscripten::val& args)
{
  bool handled = false;
  if constexpr(avnd::messages_introspection<T>::size > 0)
  {
    avnd::messages_introspection<T>::for_all(
        avnd::get_messages(obj), [&]<typename M>(M& field) {
      if(handled)
        return;
      if(name == std::string_view{M::name()})
        handled = invoke_message<M>(obj.effect, args);
    });
  }
  return handled;
}

#endif // __EMSCRIPTEN__

template <typename T>
constexpr int message_count() noexcept
{
  return (int)avnd::messages_introspection<T>::size;
}

// Arguments JS must supply for message M: reflected count, minus 1 if M takes
// the instance (`T&`) first (supplied by the backend, not the caller).
template <typename T, typename M>
constexpr int message_js_arg_count() noexcept
{
  if constexpr(std::is_void_v<avnd::message_reflection<M>>)
  {
    return 0;
  }
  else
  {
    constexpr int n = (int)avnd::message_reflection<M>::count;
    if constexpr(n > 0)
    {
      if constexpr(std::is_same_v<avnd::first_message_argument<M>, T&>)
        return n - 1;
      else
        return n;
    }
    else
    {
      return 0;
    }
  }
}

// Visit message #i (0-based) with its reflected type M. Works whether
// `messages` is declared as a type or a value member.
template <typename T, typename F>
inline void for_message_nth(avnd::effect_container<T>& obj, int i, F&& f)
{
  if constexpr(avnd::messages_introspection<T>::size > 0)
  {
    int idx = 0;
    avnd::messages_introspection<T>::for_all(
        avnd::get_messages(obj), [&]<typename M>(M& field) {
      if(idx++ == i)
        f.template operator()<M>(field);
    });
  }
}

} // namespace wasm
