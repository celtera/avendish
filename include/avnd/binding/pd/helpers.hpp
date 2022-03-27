#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <m_pd.h>

#include <string_view>

namespace pd
{
static constexpr bool valid_char_for_name(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
         || (c == '.' || c == '~');
}

template <typename T>
t_symbol* symbol_from_name()
{
  if constexpr (const char* str; requires { str = T::c_name(); })
  {
    return gensym(avnd::get_c_name<T>().data());
  }
  else
  {
    std::string name{avnd::get_name<T>()};
    for (char& c : name)
    {
      if (!valid_char_for_name(c))
        c = '_';
    }
    return gensym(name.c_str());
  }
}

template <typename T>
static void process_generic_message(T& implementation, t_symbol* s)
{
  using namespace std::literals;

  // Gets T from effect_container<T>
  using object_type = avnd::get_object_type_t<T>;

  if ("dumpall"sv == s->s_name)
  {
    int k = 0;
    avnd::for_all_inputs(
        implementation,
        [&k]<typename C>(C& ctl)
        {
          if constexpr (requires { C::name(); })
          {
            if constexpr (requires { (float)ctl.value; })
            {
              post(
                  "[dumpall] %s : %s = %f",
                  avnd::get_name<object_type>(),
                  C::name(),
                  (float)ctl.value);
            }
            else if constexpr (requires { ctl.value.c_str(); })
            {
              post(
                  "[dumpall] %s : %s = %s",
                  avnd::get_name<object_type>(),
                  C::name(),
                  ctl.value.c_str());
            }
            else if constexpr (requires { (const char*)ctl.value.data(); })
            {
              post(
                  "[dumpall] %s : %s = %s",
                  avnd::get_name<object_type>(),
                  C::name(),
                  ctl.value.data());
            }
          }
          else
          {
            if constexpr (requires { (float)ctl.value; })
            {
              post(
                  "[dumpall] %s : [%d] = %f",
                  avnd::get_name<object_type>(),
                  k,
                  (float)ctl.value);
            }
            else if constexpr (requires { ctl.value = std::string{}; })
            {
              post(
                  "[dumpall] %s : [%d] = %s",
                  avnd::get_name<object_type>(),
                  k,
                  ctl.value.c_str());
            }
          }
          k++;
        });
  }
}

template <typename Arg>
static constexpr bool compatible(t_atomtype type)
{
  if constexpr (requires(Arg arg) { arg = 0.f; })
    return type == t_atomtype::A_FLOAT;
  else if constexpr (requires(Arg arg) { arg = "str"; })
    return type == t_atomtype::A_SYMBOL;

  return false;
}

template <typename Arg>
static Arg convert(t_atom& atom)
{
  if constexpr (requires(Arg arg) { arg = 0.f; })
    return atom.a_w.w_float;
  else if constexpr (requires(Arg arg) { arg = "str"; })
    return atom.a_w.w_symbol->s_name;
  else
    static_assert(std::is_same_v<void, Arg>, "Argument type not handled yet");
}

t_symbol* symbol_for_port(avnd::float_parameter auto& port)
{
  return &s_float;
}

t_symbol* symbol_for_port(avnd::string_parameter auto& port)
{
  return &s_symbol;
}

t_symbol* symbol_for_port(avnd::mono_audio_port auto& port)
{
  return &s_signal;
}

t_symbol* symbol_for_port(avnd::poly_audio_port auto& port)
{
  return &s_signal;
}

t_symbol* symbol_for_port(auto& port)
{
  return &s_anything; // TODO is that correct ?
}

}
