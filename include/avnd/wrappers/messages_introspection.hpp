#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/message.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/common/struct_reflection.hpp>

namespace avnd
{
template <typename T>
using messages_introspection
    = fields_introspection<typename messages_type<T>::type>;

template <typename T>
auto& get_messages(avnd::effect_container<T>& t)
{
  using type = typename avnd::messages_type<T>::type;
  static const constexpr type messages;
  return messages;
}

template <avnd::messages_is_type T>
auto& get_messages(T& t)
{
  using type = typename avnd::messages_type<T>::type;
  static const constexpr type messages;
  return messages;
}

template <avnd::messages_is_value T>
auto& get_messages(T& t)
{
  return t.messages;
}

template <typename T>
static constexpr void for_all_messages(T& obj, auto&& func) noexcept
{
  if constexpr (messages_introspection<T>::size > 0)
  {
    boost::pfr::for_each_field(get_messages(obj), func);
  }
}
}
