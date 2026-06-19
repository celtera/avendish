#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// P2996 backend for the avnd enum-reflection API.

#include <avnd/common/meta_polyfill.hpp>

#include <array>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#define AVND_ENUM_REFLECTION_SUPPORTED 1

namespace avnd
{
template <typename E>
consteval std::size_t enum_count() noexcept
{
  return std::meta::enumerators_of(^^E).size();
}

template <typename E>
constexpr std::underlying_type_t<E> enum_underlying(E e) noexcept
{
  return static_cast<std::underlying_type_t<E>>(e);
}

template <typename E>
consteval std::array<E, enum_count<E>()> enum_values() noexcept
{
  std::array<E, enum_count<E>()> out{};
  std::size_t i = 0;
  for(std::meta::info m : std::meta::enumerators_of(^^E))
    out[i++] = std::meta::extract<E>(m);
  return out;
}

// Names backed by NUL-terminated static storage so .data() is a valid C string.
template <typename E>
consteval std::array<std::string_view, enum_count<E>()> enum_names() noexcept
{
  std::array<std::string_view, enum_count<E>()> out{};
  std::size_t i = 0;
  for(std::meta::info m : std::meta::enumerators_of(^^E))
    out[i++] = std::string_view{std::define_static_string(std::meta::identifier_of(m))};
  return out;
}

// (value, name) pairs.
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

// Enumerator -> name (empty if not found).
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

// name -> enumerator (nullopt if not found).
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
