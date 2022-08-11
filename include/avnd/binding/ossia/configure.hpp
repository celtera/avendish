#pragma once
#include <avnd/wrappers/configure.hpp>
#include <halp/log.hpp>
//#include <fmt/format.h>
//#include <ossia/detail/logger.hpp>
namespace oscr
{

struct logger
{
  using logger_type = logger;
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

struct config
{
  using logger_type = oscr::logger;
};

}
