#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <cstring>
#include <string>
#include <string_view>

namespace touchdesigner
{

static constexpr bool valid_char_for_touchdesigner(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

template <typename Field>
static constexpr auto get_td_name()
{
  static_constexpr auto nm = avnd::get_static_symbol<Field>();
  static_constexpr auto sz = nm.size();

  avnd::static_identifier<sz> storage{};

  bool first = true;
  int k = 0;
  for (char c : nm)
  {
    if (first)
    {
      // First character must be uppercase letter
      if (c >= 'a' && c <= 'z')
        storage[k++] = (c + ('A' - 'a'));
      else if ((c >= 'A' && c <= 'Z'))
        storage[k++] = c;
      else
        storage[k++] = 'A';
      first = false;
    }
    else
    {
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        storage[k++] = c;
      else if ((c >= 'A' && c <= 'Z'))
        storage[k++] = (c + ('a' - 'A'));
      else if ( (c >= '0' && c <= '9'))
        storage[k++] = c;
    }
  }
  storage[k] = 0;

  return storage;
}

template <typename Field>
static constexpr auto get_parameter_label()
{
  if constexpr(avnd::has_label<Field>)
    return avnd::get_label<Field>();
  else if constexpr(avnd::has_name<Field>)
    return avnd::get_name<Field>();
  else
    return get_td_name<Field>();

}
/**
 * Runtime name conversion utilities
 */
inline std::string sanitize_td_name(std::string_view name)
{
  std::string result;
  result.reserve(name.size());

  bool first = true;
  for (char c : name)
  {
    if (first)
    {
      // First character must be uppercase letter
      if (c >= 'a' && c <= 'z')
        result += (c - 'a' + 'A');
      else if ((c >= 'A' && c <= 'Z'))
        result += c;
      else
        continue; // Skip invalid first character
      first = false;
    }
    else
    {
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        result += c;
      else if ((c >= 'A' && c <= 'Z'))
        result +=  (c - 'A' + 'a');
      else if ( (c >= '0' && c <= '9'))
        result += c;
    }
  }

  if (result.empty())
    return "Param";

  return result;
}

}
