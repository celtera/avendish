#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/csound/helpers.hpp>
#include <halp/log.hpp>

#include <sstream>
#include <utility>

namespace csound
{
/**
 * Routes avnd/halp logging to the Csound console (csound->Message / ErrorMsg)
 * through the thread-local engine pointer set in helpers.hpp.
 */
struct logger
{
  template <typename... T>
  static void log(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    csound::post(str.str());
  }

  template <typename... T>
  static void error(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    csound::bug(str.str());
  }

  template <typename... T>
  static void trace(T&&... args) noexcept
  {
    logger::log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void debug(T&&... args) noexcept
  {
    logger::log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void info(T&&... args) noexcept
  {
    logger::log(std::forward<T>(args)...);
  }
  template <typename... T>
  static void warn(T&&... args) noexcept
  {
    logger::error(std::forward<T>(args)...);
  }
  template <typename... T>
  static void critical(T&&... args) noexcept
  {
    logger::error(std::forward<T>(args)...);
  }
};

static_assert(avnd::logger<csound::logger>);

struct config
{
  using logger_type = csound::logger;
};
}
