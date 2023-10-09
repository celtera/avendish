#pragma once

#include <avnd/concepts/parameter.hpp>

#include <ext.h>
#include <string_view>

namespace max
{

template<typename>
inline t_symbol* get_atoms_sym() noexcept
{
  return USESYM(atom);
}
template<std::integral T>
inline t_symbol* get_atoms_sym() noexcept
{
  return USESYM(long);
}
/*
template<>
inline t_symbol* get_atoms_sym<bool>() noexcept
{
  return USESYM(long);
}
*/
template<>
inline t_symbol* get_atoms_sym<float>() noexcept
{
  return USESYM(float32);
}
template<>
inline t_symbol* get_atoms_sym<double>() noexcept
{
  return USESYM(float64);
}
template<>
inline t_symbol* get_atoms_sym<char>() noexcept
{
  return USESYM(char);
}
template<>
inline t_symbol* get_atoms_sym<std::string_view>() noexcept
{
  return USESYM(symbol);
}
template<>
inline t_symbol* get_atoms_sym<std::string>() noexcept
{
  return USESYM(symbol);
}


template<typename F>
constexpr const char* get_atoms_style() noexcept
{
  using V = decltype(F::value);
  if constexpr(std::is_same_v<V, bool>)
    return "onoff";
  if constexpr(std::is_enum_v<V>)
    return "enumindex";
  if constexpr(std::is_same_v<V, std::string>)
    return "text";
  if constexpr(std::is_arithmetic_v<V>)
    return "";
  if constexpr(avnd::rgb_value<V>)
    return "rgba";

  // TODO:
  //  "enum"
  //  "rect"
  //  "font"
  //  "file"
  return "";
}
}
