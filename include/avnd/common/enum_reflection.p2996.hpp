#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// C++26 static-reflection (P2996) implementation of the avnd enum-reflection API.
// Mirrors the subset of the magic_enum API that avendish relies on, so that
// magic_enum becomes an optional dependency on reflecting compilers.

#include <avnd/common/meta_polyfill.hpp>

#include <array>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#define AVND_ENUM_REFLECTION_SUPPORTED 1

namespace avnd
{
// Number of enumerators in E.
template <typename E>
consteval std::size_t enum_count() noexcept
{
  return std::meta::enumerators_of(^^E).size();
}

// Underlying integral value of an enumerator (runtime-callable).
template <typename E>
constexpr std::underlying_type_t<E> enum_underlying(E e) noexcept
{
  return static_cast<std::underlying_type_t<E>>(e);
}

// All enumerator values, in declaration order.
template <typename E>
consteval std::array<E, enum_count<E>()> enum_values() noexcept
{
  std::array<E, enum_count<E>()> out{};
  std::size_t i = 0;
  for(std::meta::info m : std::meta::enumerators_of(^^E))
    out[i++] = std::meta::extract<E>(m);
  return out;
}

// All enumerator names, in declaration order.
// Names are materialized through define_static_string so the resulting
// string_views are backed by NUL-terminated static storage: callers that do
// enum_name(v).data() (gensym, godot::String, ...) get a valid C string,
// matching magic_enum's behaviour.
template <typename E>
consteval std::array<std::string_view, enum_count<E>()> enum_names() noexcept
{
  std::array<std::string_view, enum_count<E>()> out{};
  std::size_t i = 0;
  for(std::meta::info m : std::meta::enumerators_of(^^E))
    out[i++] = std::string_view{std::define_static_string(std::meta::identifier_of(m))};
  return out;
}

// (value, name) pairs — mirrors magic_enum::enum_entries (std::pair<E, string_view>).
template <typename E>
consteval std::array<std::pair<E, std::string_view>, enum_count<E>()>
enum_entries() noexcept
{
  std::array<std::pair<E, std::string_view>, enum_count<E>()> out{};
  std::size_t i = 0;
  for(std::meta::info m : std::meta::enumerators_of(^^E))
  {
    out[i] = std::pair<E, std::string_view>{
        std::meta::extract<E>(m),
        std::string_view{std::define_static_string(std::meta::identifier_of(m))}};
    ++i;
  }
  return out;
}

// Enumerator -> name (runtime-callable; empty string_view if not found).
template <typename E>
constexpr std::string_view enum_name(E e) noexcept
{
  constexpr auto vals = enum_values<E>();
  constexpr auto names = enum_names<E>();
  for(std::size_t i = 0; i < vals.size(); ++i)
    if(vals[i] == e)
      return names[i];
  return {};
}

// name -> enumerator (runtime-callable; nullopt if not found).
template <typename E>
constexpr std::optional<E> enum_cast(std::string_view name) noexcept
{
  constexpr auto vals = enum_values<E>();
  constexpr auto names = enum_names<E>();
  for(std::size_t i = 0; i < vals.size(); ++i)
    if(names[i] == name)
      return vals[i];
  return std::nullopt;
}
}
