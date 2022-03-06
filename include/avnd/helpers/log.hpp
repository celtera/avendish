#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if __has_include(<fmt/printf.h>)
#include <fmt/format.h>
#include <fmt/printf.h>
#else
#include <iostream>
#endif

namespace avnd
{
template <typename T>
concept logger = requires(T t) {
#if defined(__APPLE__)
                   T{};
#else
#if defined(__clang__) && (__clang_major__ >= 12) && (__clang_major__ < 14)
                   &T::log;
                   &T::error;
#else
                   t.log("{} {}", 1, "foo");
                   t.error("{} {}", 1, "foo");
#endif
#endif
                 };

#if defined(FMT_PRINTF_H_)
struct basic_logger
{
  template <typename... T>
  static void log(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    fmt::print(stdout, fmt, std::forward<T>(args)...);
    fputc('\n', stdout);
    std::fflush(stdout);
  }

  template <typename... T>
  static void error(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    fmt::print(stderr, fmt, std::forward<T>(args)...);
    fputc('\n', stderr);
    std::fflush(stderr);
  }
};
#else
struct basic_logger
{
  template <typename... T>
  static void log(T&&... args) noexcept
  {
    ((std::cout << args), ...);
    std::cout << std::endl;
  }

  template <typename... T>
  static void error(T&&... args) noexcept
  {
    ((std::cerr << args), ...);
    std::cerr << std::endl;
  }
};
#endif

struct no_logger
{
  template <typename... T>
  static void log(T&&... args) noexcept
  {
  }

  template <typename... T>
  static void error(T&&... args) noexcept
  {
  }
};

static_assert(avnd::logger<basic_logger>);
static_assert(avnd::logger<no_logger>);

template <typename C>
concept has_logger = requires {
                       {
                         typename C::logger_type{}
                         } -> avnd::logger;
                     };
}
