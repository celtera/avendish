#pragma once
#include <ext.h>
#undef post
#undef error

#include <halp/log.hpp>
#include <avnd/wrappers/configure.hpp>

#include <utility>

#if !defined(FMT_PRINTF_H_)
#include <ostream>
#include <sstream>
#endif
namespace max
{
#if defined(FMT_PRINTF_H_)
struct logger
{
  template <typename... T>
  static void log(fmt::format_string<T...> fmt, T&&... args)
  {
    object_post(NULL, "%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
  }

  template <typename... T>
  static void error(fmt::format_string<T...> fmt, T&&... args)
  {
    object_error(NULL, "%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
  }
  template <typename... T>
  static void trace(fmt::format_string<T...> fmt, T&&... args) noexcept { log(fmt, std::forward<T>(args)...); }
  template <typename... T>
  static void debug(fmt::format_string<T...> fmt, T&&... args) noexcept { log(fmt, std::forward<T>(args)...); }
  template <typename... T>
  static void info(fmt::format_string<T...> fmt, T&&... args) noexcept { log(fmt, std::forward<T>(args)...); }
  template <typename... T>
  static void warn(fmt::format_string<T...> fmt, T&&... args) noexcept { error(fmt, std::forward<T>(args)...); }
  template <typename... T>
  static void critical(fmt::format_string<T...> fmt, T&&... args) noexcept { error(fmt, std::forward<T>(args)...); }
};
#else
struct logger
{
  template <typename... T>
  static void log(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    object_post(NULL, "%s", str.str().c_str());
  }

  template <typename... T>
  static void error(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    object_error(NULL, "%s", str.str().c_str());
  }
  template <typename... T>
  static void trace(T&&... args) noexcept { log(std::forward<T>(args)...); }
  template <typename... T>
  static void debug(T&&... args) noexcept { log(std::forward<T>(args)...); }
  template <typename... T>
  static void info(T&&... args) noexcept { log(std::forward<T>(args)...); }
  template <typename... T>
  static void warn(T&&... args) noexcept { error(std::forward<T>(args)...); }
  template <typename... T>
  static void critical(T&&... args) noexcept { error(std::forward<T>(args)...); }
};
#endif

static_assert(avnd::logger<max::logger>);

struct config
{
  using logger_type = max::logger;
};

}
