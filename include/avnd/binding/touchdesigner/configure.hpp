#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/wrappers/configure.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <halp/log.hpp>

#include <CPlusPlus_Common.h>
#include <sstream>
#include <utility>

// Forward declare TouchDesigner types to avoid requiring headers during configuration
namespace TD {
// These will be properly included in implementation files
}

namespace touchdesigner
{

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


template<typename type>
inline void configure_opInfo(TD::OP_CustomOPInfo& op, std::string_view nm, std::string_view optype) {

  // TD rules:
  // - first letter is always capital letter
  // - then only letters and numbers
  // - more than 3 chars
  std::string nm2 = sanitize_td_name(nm);
  op.opType->setString(nm2.c_str());
  op.opLabel->setString(nm2.c_str());
  char icon[4]{nm2[0],nm2[1],nm2[2],0};
  op.opIcon->setString(icon);

  if constexpr (avnd::has_name<type>)
  {
    op.opLabel->setString(avnd::get_name<type>().data());
  }

  if constexpr (avnd::has_author<type>)
  {
    op.authorName->setString(avnd::get_author<type>().data());
  }
  else
  {
    op.authorName->setString("Author");
  }

  if constexpr (avnd::has_email<type>)
  {
    op.authorEmail->setString(avnd::get_email<type>().data());
  }
  else
  {
    op.authorEmail->setString("mail@foo.bar");
  }

  // FIXME
  if(optype == "CHOP") {
    op.minInputs = avnd::bus_introspection<type>::input_busses;
    op.maxInputs = op.minInputs;
  }
  else
  {
    op.minInputs = avnd::value_port_input_introspection<type>::size;
    op.maxInputs = op.minInputs;
  }
}
}
