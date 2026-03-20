#pragma once
#include <avnd/wrappers/configure.hpp>
#include <halp/log.hpp>
#include <m_pd.h>

#include <utility>

#if !defined(FMT_PRINTF_H_)
#include <ostream>
#include <sstream>
#endif
namespace pd
{

#if defined(FMT_PRINTF_H_)
struct logger
{
  template <typename... T>
  static void log(fmt::format_string<T...> fmt, T&&... args)
  {
    ::post("%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
  }

  template <typename... T>
  static void error(fmt::format_string<T...> fmt, T&&... args)
  {
    ::bug("%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
  }
  template <typename... T>
  static void trace(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    logger::log(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void debug(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    logger::log(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void info(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    logger::log(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void warn(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    logger::error(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void critical(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    logger::error(fmt, std::forward<T>(args)...);
  }
};
#else
struct logger
{
  template <typename... T>
  static void log(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    ::post("%s", str.str().c_str());
  }

  template <typename... T>
  static void error(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    ::bug("%s", str.str().c_str());
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
#endif

static_assert(avnd::logger<pd::logger>);

struct config
{
  using logger_type = pd::logger;
};

}
