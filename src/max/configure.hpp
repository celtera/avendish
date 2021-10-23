#pragma once
#include <ext.h>
#undef post
#undef error

#include <avnd/configure.hpp>
#include <helpers/log.hpp>

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
  void log(fmt::format_string<T...> fmt, T&&... args)
  {
    object_post(
        NULL, "%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
  }

  template <typename... T>
  void error(fmt::format_string<T...> fmt, T&&... args)
  {
    object_error(
        NULL, "%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
  }
};
#else
struct logger
{
  template <typename... T>
  void log(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    object_post(NULL, "%s", str.str().c_str());
  }

  template <typename... T>
  void error(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    object_error(NULL, "%s", str.str().c_str());
  }
};
#endif

static_assert(avnd::logger<max::logger>);

struct config
{
  using logger_type = max::logger;
};

}
