#pragma once
#include <fmt/format.h>
namespace gst
{
struct logger
{
  template <typename... T>
  static void log(fmt::format_string<T...> fmt, T&&... args)
  {
  }

  template <typename... T>
  static void error(fmt::format_string<T...> fmt, T&&... args)
  {
  }
  template <typename... T>
  static void trace(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
  }
  template <typename... T>
  static void debug(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
  }
  template <typename... T>
  static void info(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
  }
  template <typename... T>
  static void warn(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
  }
  template <typename... T>
  static void critical(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
  }
};
struct config
{
  using logger_type = gst::logger;
};
}
