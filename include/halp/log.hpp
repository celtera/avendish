#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/logger.hpp>

#include <cinttypes>
#include <string_view>
#include <utility>

#if __has_include(<fmt/format.h>) && !defined(AVND_DISABLE_FMT)
#include <fmt/format.h>
#include <fmt/printf.h>
#else
#include <cstdio>
#endif

#include <halp/modules.hpp>

HALP_MODULE_EXPORT
namespace halp
{
#if defined(FMT_PRINTF_H_)
struct basic_logger
{
  using logger_type = basic_logger;
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
  template <typename... T>
  static void trace(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    log(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void debug(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    log(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void info(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    log(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void warn(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    error(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void critical(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    error(fmt, std::forward<T>(args)...);
  }
};
#else
struct basic_logger
{
  static void do_print(int64_t x) { fprintf(stderr, "%" PRId64, x); }
  static void do_print(double x) { fprintf(stderr, "%f", x); }
  static void do_print(std::string_view x)
  {
    fprintf(stderr, "%.*s", (int)x.size(), x.data());
  }
  static void do_print(bool x) { fprintf(stderr, "%s", x ? "true" : "false"); }
  static void do_print(const auto& other) { fprintf(stderr, "??"); }

  using logger_type = basic_logger;
  template <typename... T>
  static void log(T&&... args) noexcept
  {
    (do_print(args), ...);
    fprintf(stderr, "\n");
  }

  template <typename... T>
  static void trace(T&&... args) noexcept
  {
    log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void debug(T&&... args) noexcept
  {
    log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void info(T&&... args) noexcept
  {
    log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void warn(T&&... args) noexcept
  {
    log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void error(T&&... args) noexcept
  {
    log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void critical(T&&... args) noexcept
  {
    log(std::forward<T>(args)...);
  }
};
#endif

struct no_logger
{
  using logger_type = no_logger;
  template <typename... T>
  static void log(T&&... args) noexcept
  {
  }
  template <typename... T>
  static void trace(T&&... args) noexcept
  {
  }
  template <typename... T>
  static void debug(T&&... args) noexcept
  {
  }
  template <typename... T>
  static void info(T&&... args) noexcept
  {
  }
  template <typename... T>
  static void warn(T&&... args) noexcept
  {
  }
  template <typename... T>
  static void error(T&&... args) noexcept
  {
  }
  template <typename... T>
  static void critical(T&&... args) noexcept
  {
  }
};

static_assert(avnd::logger<halp::basic_logger>);
static_assert(avnd::logger<halp::no_logger>);

template <typename C>
concept has_logger = avnd::logger<typename C::logger_type>;
}
