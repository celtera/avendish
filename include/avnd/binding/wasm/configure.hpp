#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/logger.hpp>
#include <avnd/wrappers/configure.hpp>

#include <cstdio>
#include <sstream>
#include <utility>

#if defined(__EMSCRIPTEN__)
#include <emscripten/console.h>
#endif

namespace wasm
{
/**
 * Logger satisfying avnd::logger. Routes to the browser console on emscripten,
 * stderr/stdout natively. Non-fmt variadic form, like pd::logger's non-fmt branch.
 */
struct logger
{
  template <typename... T>
  static std::string concat(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    return std::move(str).str();
  }

#if defined(__EMSCRIPTEN__)
  template <typename... T>
  static void log(T&&... args)
  {
    emscripten_console_log(concat(std::forward<T>(args)...).c_str());
  }
  template <typename... T>
  static void error(T&&... args)
  {
    emscripten_console_error(concat(std::forward<T>(args)...).c_str());
  }
  template <typename... T>
  static void warn(T&&... args)
  {
    emscripten_console_warn(concat(std::forward<T>(args)...).c_str());
  }
#else
  template <typename... T>
  static void log(T&&... args)
  {
    auto s = concat(std::forward<T>(args)...);
    s += '\n';
    std::fputs(s.c_str(), stdout);
  }
  template <typename... T>
  static void error(T&&... args)
  {
    auto s = concat(std::forward<T>(args)...);
    s += '\n';
    std::fputs(s.c_str(), stderr);
  }
  template <typename... T>
  static void warn(T&&... args)
  {
    error(std::forward<T>(args)...);
  }
#endif

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
  static void critical(T&&... args) noexcept
  {
    error(std::forward<T>(args)...);
  }
};

static_assert(avnd::logger<wasm::logger>);

struct config
{
  using logger_type = wasm::logger;
};

}
