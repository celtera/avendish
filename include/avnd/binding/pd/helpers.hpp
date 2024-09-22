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
  if constexpr(const char* str; requires { str = T::c_name(); })
  {
    return gensym(avnd::get_c_name<T>().data());
  }
  else
  {
    std::string name{avnd::get_name<T>()};
    for(char& c : name)
    {
      if(!valid_char_for_name(c))
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

t_symbol* symbol_for_port(avnd::mono_audio_port auto& port)
{
  return &s_signal;
}

t_symbol* symbol_for_port(avnd::poly_audio_port auto& port)
{
  return &s_signal;
}

t_symbol* symbol_for_port(avnd::parameter auto& port)
{
  using type = std::remove_cvref_t<decltype(port.value)>;
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
  else if constexpr(avnd::span_ish<type>)
    return &s_list;
  else if constexpr(std::floating_point<type>)
    return &s_float;
  else if constexpr(std::integral<type>)
    return &s_float;
  else if constexpr(avnd::string_ish<type>)
    return &s_symbol;
  return &s_anything; // TODO is that correct ?
}

t_symbol* symbol_for_port(auto& port)
{
  return &s_anything; // TODO is that correct ?
}
}
