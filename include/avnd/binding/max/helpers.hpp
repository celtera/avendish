#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <ext.h>
#include <ext_obex.h>

#include <string_view>

namespace max
{
template <typename T>
concept convertible_to_fundamental_value_type
    = std::is_arithmetic_v<T> || avnd::string_ish<T> || std::is_same_v<const char*, T> || std::is_same_v<std::string_view, T>;
template<typename T>
struct convertible_to_fundamental_value_type_pred : std::bool_constant<convertible_to_fundamental_value_type<T>>{};
template <typename T>
concept convertible_to_atom_list_statically
    = convertible_to_fundamental_value_type<T> ||
      (avnd::bitset_ish<T>) ||
      (avnd::span_ish<T> && convertible_to_fundamental_value_type<typename T::value_type>) ||
      (avnd::pair_ish<T> && convertible_to_fundamental_value_type<typename T::first_type> && convertible_to_fundamental_value_type<typename T::second_type>) ||
      (avnd::map_ish<T> && convertible_to_fundamental_value_type<typename T::key_type> && convertible_to_fundamental_value_type<typename T::mapped_type>) ||
      (avnd::optional_ish<T> && convertible_to_fundamental_value_type<typename T::value_type>) ||
      (avnd::variant_ish<T> && boost::mp11::mp_all_of<T, convertible_to_fundamental_value_type_pred>::value) ||
      (avnd::tuple_ish<T> && boost::mp11::mp_all_of<T, convertible_to_fundamental_value_type_pred>::value)
    ;

static constexpr bool valid_char_for_name(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
         || (c == '.' || c == '~');
}

template <typename T>
static constexpr auto get_name_symbol()
{
  return avnd::get_static_symbol<T, max::valid_char_for_name>();
}

template <typename T>
t_symbol* symbol_from_name()
{
  return gensym(max::get_name_symbol<T>().data());
}


template <typename T>
static void process_generic_message(T& implementation, t_symbol* s)
{
  using namespace std::literals;

  // Gets T from effect_container<T>
  using object_type = avnd::get_object_type_t<T>;

  if("dumpall"sv == s->s_name)
  {
    int k = 0;
    avnd::input_introspection<object_type>::for_all(
        avnd::get_inputs<object_type>(implementation), [&k]<typename C>(C& ctl) {
          constexpr auto obj_name = avnd::get_name<object_type>().data();
          if constexpr(requires { C::name(); })
          {
            constexpr auto ctl_name = avnd::get_name<C>().data();
            if constexpr(requires { (float)ctl.value; })
            {
              post("[dumpall] %s : %s = %f", obj_name, ctl_name, (float)ctl.value);
            }
            else if constexpr(requires { ctl.value.c_str(); })
            {
              post("[dumpall] %s : %s = %s", obj_name, ctl_name, ctl.value.c_str());
            }
            else if constexpr(requires { (const char*)ctl.value.data(); })
            {
              post("[dumpall] %s : %s = %s", obj_name, ctl_name, ctl.value.data());
            }
          }
          else
          {
            if constexpr(requires { (float)ctl.value; })
            {
              post("[dumpall] %s : [%d] = %f", obj_name, k, (float)ctl.value);
            }
            else if constexpr(requires { ctl.value = std::string{}; })
            {
              post("[dumpall] %s : [%d] = %s", obj_name, k, ctl.value.c_str());
            }
          }
          k++;
        });
  }
}

template <typename Arg>
static constexpr bool compatible(short type)
{
  // NOTE! has to be in this order as std::string x = 0.f; builds...
  if constexpr(requires(Arg arg) { arg = "str"; })
    return type == A_SYM;
  else if constexpr(requires(Arg arg) { arg = 0.f; })
    return type == A_FLOAT || type == A_LONG;

  return false;
}

template <typename Arg>
static Arg convert(t_atom& atom)
{
  if constexpr(requires(Arg arg) { arg = "str"; })
    return atom.a_w.w_sym->s_name;
  else if constexpr(std::floating_point<Arg>)
    return atom.a_type == A_FLOAT ? atom.a_w.w_float : atom.a_w.w_long;
  else if constexpr(std::integral<Arg>)
    return atom.a_type == A_LONG ? atom.a_w.w_long : atom.a_w.w_float;

  else
    static_assert(std::is_same_v<void, Arg>, "Argument type not handled yet");
}

template<avnd::int_parameter C>
t_symbol* symbol_for_port()
{
  return _sym_long;
}

template<avnd::float_parameter C>
t_symbol* symbol_for_port()
{
  return _sym_float;
}

template<avnd::string_parameter C>
t_symbol* symbol_for_port()
{
  return _sym_symbol;
}

template<avnd::mono_audio_port C>
t_symbol* symbol_for_port()
{
  return _sym_signal;
}

template<typename C>
t_symbol* symbol_for_port()
{
  return _sym_anything; // TODO is that correct ?
}

template <typename R, typename... Args, template <typename...> typename F>
static t_symbol* symbol_for_arguments(F<R(Args...)>)
{
  if constexpr(sizeof...(Args) == 0)
  {
    return _sym_bang;
  }
  else if constexpr(sizeof...(Args) == 1)
  {
    if constexpr(std::is_integral_v<Args...>)
      return _sym_long;
    else if constexpr(std::is_floating_point_v<Args...>)
      return _sym_float;
    else if constexpr(std::is_convertible_v<Args..., std::string>)
      return _sym_symbol;
    else
      return _sym_list;
  }
  else
  {
    return _sym_list;
  }
}

template <typename C>
static t_symbol* get_message_out_symbol()
{
  if constexpr(avnd::has_symbol<C>)
  {
    static constexpr auto sym = avnd::get_symbol<C>();
    return gensym(sym.data());
  }
  else if constexpr(avnd::has_c_name<C>)
  {
    static constexpr auto sym = avnd::get_c_name<C>();
    return gensym(sym.data());
  }
  else if constexpr(requires { sizeof(decltype(C::call)); })
  {
    using call_type = decltype(C::call);
    return symbol_for_arguments(call_type{});
  }
  else
  {
    using value_type = decltype(C::value);
    return symbol_for_port<C>();
  }
}

}
