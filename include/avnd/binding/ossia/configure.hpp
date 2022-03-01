#pragma once
#include <avnd/wrappers/configure.hpp>
#include <fmt/format.h>
//#include <ossia/detail/logger.hpp>
namespace oscr
{

struct logger
{
  template <typename... T>
  void log(fmt::format_string<T...> fmt, T&&... args)
  {
  //  ossia::logger().info(fmt, std::forward<T>(args)...);
  }

  template <typename... T>
  void error(fmt::format_string<T...> fmt, T&&... args)
  {
  //  ossia::logger().error(fmt, std::forward<T>(args)...);
  }
};

struct config
{
  using logger_type = oscr::logger;
};

}
