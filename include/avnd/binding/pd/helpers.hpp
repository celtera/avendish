#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <m_pd.h>

#include <string>
#include <string_view>

namespace pd
{
template <typename T>
concept convertible_to_fundamental_value_type_impl
    = std::is_arithmetic_v<T> || std::is_enum_v<T> || avnd::string_ish<T> || std::is_same_v<const char*, T> || std::is_same_v<std::string_view, T>;
template <typename T>
concept convertible_to_fundamental_value_type
    = convertible_to_fundamental_value_type_impl<std::remove_cvref_t<T>>;

template<typename T>
struct convertible_to_fundamental_value_type_pred : std::bool_constant<convertible_to_fundamental_value_type<T>>{};

// clang-format off
template <typename T>
concept convertible_to_atom_list_statically_impl
    = convertible_to_fundamental_value_type<T> ||
      (avnd::bitset_ish<T>) ||
      (avnd::iterable_ish<T> && convertible_to_fundamental_value_type<typename T::value_type>) ||
      (avnd::pair_ish<T> && convertible_to_fundamental_value_type<typename T::first_type> && convertible_to_fundamental_value_type<typename T::second_type>) ||
      (avnd::map_ish<T> && convertible_to_fundamental_value_type<typename T::key_type> && convertible_to_fundamental_value_type<typename T::mapped_type>) ||
      (avnd::optional_ish<T> && convertible_to_fundamental_value_type<typename T::value_type>) ||
      (avnd::variant_ish<T> && boost::mp11::mp_all_of<T, convertible_to_fundamental_value_type_pred>::value) ||
      (avnd::tuple_ish<T> && boost::mp11::mp_all_of<T, convertible_to_fundamental_value_type_pred>::value) ||
      (std::is_aggregate_v<T> && !avnd::iterable_ish<T> && boost::mp11::mp_all_of<avnd::as_typelist<T>, convertible_to_fundamental_value_type_pred>::value)
    ;
// clang-format on
template <typename T>
concept convertible_to_atom_list_statically
    = convertible_to_atom_list_statically_impl<std::remove_cvref_t<T>>;

static constexpr bool valid_char_for_name(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
         || (c == '.' || c == '~');
}

template <typename T>
static constexpr auto get_name_symbol()
{
  return avnd::get_static_symbol<T, pd::valid_char_for_name>();
}

template <typename T>
t_symbol* symbol_from_name()
{
  return gensym(pd::get_name_symbol<T>().data());
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
          if constexpr(avnd::has_name<C>)
          {
            if constexpr(requires { (float)ctl.value; })
            {
              post(
                  "[dumpall] %s : %s = %f", obj_name, avnd::get_name<C>().data(),
                  (float)ctl.value);
            }
            else if constexpr(requires { ctl.value.c_str(); })
            {
              post(
                  "[dumpall] %s : %s = %s", obj_name, avnd::get_name<C>().data(),
                  ctl.value.c_str());
            }
            else if constexpr(requires { (const char*)ctl.value.data(); })
            {
              post(
                  "[dumpall] %s : %s = %s", obj_name, avnd::get_name<C>().data(),
                  ctl.value.data());
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
static constexpr bool compatible(t_atomtype type)
{
  if constexpr(requires(Arg arg) { arg = "str"; })
    return type == t_atomtype::A_SYMBOL;
  else if constexpr(requires(Arg arg) { arg = 0.f; })
    return type == t_atomtype::A_FLOAT;

  return false;
}

template <typename Arg>
static Arg convert(t_atom& atom)
{
  if constexpr(requires(Arg arg) { arg = "str"; })
    return atom.a_w.w_symbol->s_name;
  else if constexpr(requires(Arg arg) { arg = 0.f; })
    return atom.a_w.w_float;
  else
    static_assert(std::is_same_v<void, Arg>, "Argument type not handled yet");
}

template <typename T>
t_symbol* symbol_for_port()
{
  return &s_anything; // TODO is that correct ?
}

template <avnd::mono_audio_port P>
t_symbol* symbol_for_port()
{
  return &s_signal;
}

template <avnd::poly_audio_port P>
t_symbol* symbol_for_port()
{
  return &s_signal;
}

template <avnd::parameter P>
t_symbol* symbol_for_port()
{
  using type = std::remove_cvref_t<decltype(P::value)>;
  if constexpr(avnd::vector_ish<type>)
    return &s_list;
  else if constexpr(avnd::set_ish<type>)
    return &s_list;
  else if constexpr(avnd::map_ish<type>)
    return &s_list;
  else if constexpr(avnd::pair_ish<type>)
    return &s_list;
  else if constexpr(avnd::tuple_ish<type>)
    return &s_list;
  else if constexpr(avnd::iterable_ish<type>)
    return &s_list;
  else if constexpr(std::floating_point<type>)
    return &s_float;
  else if constexpr(std::integral<type>)
    return &s_float;
  else if constexpr(avnd::string_ish<type>)
    return &s_symbol;
  return &s_anything; // TODO is that correct ?
}

template <typename R, typename... Args, template <typename...> typename F>
static t_symbol* symbol_for_arguments(F<R(Args...)>)
{
  if constexpr(sizeof...(Args) == 0)
  {
    return &s_bang;
  }
  else if constexpr(sizeof...(Args) == 1)
  {
    if constexpr(std::is_integral_v<Args...> || std::is_floating_point_v<Args...>)
      return &s_float;
    else if constexpr(std::is_convertible_v<Args..., std::string>)
      return &s_symbol;
    else
      return &s_list;
  }
  else
  {
    return &s_list;
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
