#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/configure.hpp>
#include <halp/log.hpp>

#include <sstream>
#include <utility>

// Forward declare TouchDesigner types to avoid requiring headers during configuration
namespace TD {
  // These will be properly included in implementation files
}

namespace touchdesigner
{

/**
 * Logger for TouchDesigner that uses the standard error/post functions
 * Note: We don't include TD headers here to keep configure lightweight
 * Actual logging will be done via function pointers set at runtime
 */
struct logger
{
  // Function pointers for TD logging (set by binding initialization)
  static inline void (*log_func)(const char*) = nullptr;
  static inline void (*error_func)(const char*) = nullptr;

  template <typename... T>
  static void log(T&&... args)
  {
    if (!log_func)
      return;

    std::ostringstream str;
    ((str << args), ...);
    log_func(str.str().c_str());
  }

  template <typename... T>
  static void error(T&&... args)
  {
    if (!error_func)
      return;

    std::ostringstream str;
    ((str << args), ...);
    error_func(str.str().c_str());
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
    error(std::forward<T>(args)...);
  }

  template <typename... T>
  static void critical(T&&... args) noexcept
  {
    error(std::forward<T>(args)...);
  }
};

static_assert(avnd::logger<touchdesigner::logger>);

/**
 * Configuration type for TouchDesigner bindings
 */
struct config
{
  using logger_type = touchdesigner::logger;
};

} // namespace touchdesigner
