#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// magic_enum backend for the avnd enum-reflection API (no-reflection fallback).

#if __has_include(<magic_enum.hpp>)
#include <magic_enum.hpp>
#elif __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#error magic_enum is required (or build with C++26 reflection, e.g. -freflection)
#endif

#include <optional>
#include <string_view>

// 0 on toolchains where magic_enum is unsupported.
#define AVND_ENUM_REFLECTION_SUPPORTED MAGIC_ENUM_SUPPORTED

namespace avnd
{
template <typename E>
constexpr std::size_t enum_count() noexcept
{
  return magic_enum::enum_count<E>();
}

template <typename E>
constexpr auto enum_underlying(E e) noexcept
{
  return magic_enum::enum_underlying(e);
}

template <typename E>
constexpr auto enum_values() noexcept
{
  return magic_enum::enum_values<E>();
}

template <typename E>
constexpr auto enum_names() noexcept
{
  return magic_enum::enum_names<E>();
}

template <typename E>
constexpr auto enum_entries() noexcept
{
  return magic_enum::enum_entries<E>();
}

template <typename E>
constexpr std::string_view enum_name(E e) noexcept
{
  return magic_enum::enum_name(e);
}

template <typename E>
constexpr std::optional<E> enum_cast(std::string_view name) noexcept
{
  return magic_enum::enum_cast<E>(name);
}
}
