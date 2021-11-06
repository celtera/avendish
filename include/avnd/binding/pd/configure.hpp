#pragma once
#include <avnd/wrappers/configure.hpp>
#include <avnd/helpers/log.hpp>
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
  void log(fmt::format_string<T...> fmt, T&&... args)
  {
    post("%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
  }

  template <typename... T>
  void error(fmt::format_string<T...> fmt, T&&... args)
  {
    error("%s", fmt::format(fmt, std::forward<T>(args)...).c_str());
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
    post("%s", str.str().c_str());
  }

  template <typename... T>
  void error(T&&... args)
  {
    std::ostringstream str;
    ((str << args), ...);
    error("%s", str.str().c_str());
  }
};
#endif

static_assert(avnd::logger<pd::logger>);

struct config
{
  using logger_type = pd::logger;
};

}
