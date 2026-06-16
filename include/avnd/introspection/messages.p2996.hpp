#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// C++26 reflection backend for message introspection: discovers plain member
// functions tagged with [[=halp::message]] / [[=halp::message_named(...)]] and
// exposes them through the SAME interface the existing bindings already consume
// (avnd::stateless_message + field_reflection<Index, MessageType>), so a message
// declared as an annotated method is indistinguishable from one declared the
// classic way once it reaches the binding layer.

#include <avnd/common/meta_polyfill.hpp>

#if AVND_USE_STD_REFLECTION

#include <avnd/common/index.hpp>
#include <avnd/common/message_tag.hpp>

#include <cstddef>
#include <string_view>

namespace avnd
{
namespace detail
{
// access_context::unchecked() lets us see all members without depending on the
// caller's access context (and avoids ODR issues with a stored constexpr ctx).
consteval std::meta::info message_member_type(std::meta::info a) noexcept
{
  // Annotation types come back cv-qualified on some compilers (e.g. GCC reports
  // `const avnd::message_annotation`); strip it before comparing.
  return std::meta::remove_cv(std::meta::type_of(a));
}

consteval bool is_message_annotation(std::meta::info a) noexcept
{
  std::meta::info t = message_member_type(a);
  if(t == ^^avnd::message_annotation)
    return true;
  return std::meta::has_template_arguments(t)
         && std::meta::template_of(t) == ^^avnd::named_message_annotation;
}

template <std::meta::info R>
consteval bool is_annotated_message() noexcept
{
  for(std::meta::info a : std::meta::annotations_of(R))
    if(is_message_annotation(a))
      return true;
  return false;
}

consteval bool is_candidate_member(std::meta::info m) noexcept
{
  return std::meta::is_function(m) && !std::meta::is_special_member_function(m)
         && !std::meta::is_static_member(m);
}
}

// Wraps an annotated member function so it models avnd::stateless_message:
// name() is the annotation's explicit name (if any) else the identifier, and
// func() is a pointer-to-member synthesized from the reflection.
template <std::meta::info R>
struct reflected_message
{
  static consteval std::string_view name() noexcept
  {
    // Default: the function identifier. An explicit [[=halp::message_named(...)]]
    // overrides it. Each candidate is materialized into static storage via
    // define_static_string immediately, so no view dangles past the temporary.
    std::string_view nm = std::define_static_string(std::meta::identifier_of(R));
    template for(constexpr std::meta::info a :
                 std::define_static_array(std::meta::annotations_of(R)))
    {
      constexpr std::meta::info t = detail::message_member_type(a);
      if constexpr(std::meta::has_template_arguments(t)
                   && std::meta::template_of(t) == ^^avnd::named_message_annotation)
      {
        constexpr auto value = std::meta::extract<typename[:t:]>(a);
        nm = std::define_static_string(value.name());
      }
    }
    return nm;
  }

  static consteval auto func() noexcept { return &[:R:]; }
};

// Number of [[=halp::message]] member functions on T.
template <typename T>
consteval std::size_t annotated_message_count() noexcept
{
  std::size_t n = 0;
  template for(constexpr std::meta::info m : std::define_static_array(
                   std::meta::members_of(^^T, std::meta::access_context::unchecked())))
  {
    if constexpr(detail::is_candidate_member(m))
      if constexpr(detail::is_annotated_message<m>())
        ++n;
  }
  return n;
}

// Mirrors fields_introspection's static surface (size + for_all) for the
// annotated-message set. for_all yields field_reflection<0, reflected_message<R>>,
// matching what bindings already expect (Field::type is the message type).
template <typename T>
struct annotated_messages_introspection
{
  static constexpr std::size_t size = annotated_message_count<T>();

  static constexpr void for_all(auto&& func)
  {
    template for(constexpr std::meta::info m : std::define_static_array(
                     std::meta::members_of(^^T, std::meta::access_context::unchecked())))
    {
      if constexpr(detail::is_candidate_member(m))
        if constexpr(detail::is_annotated_message<m>())
          func(avnd::field_reflection<0, reflected_message<m>>{});
    }
  }
};
}

#endif
