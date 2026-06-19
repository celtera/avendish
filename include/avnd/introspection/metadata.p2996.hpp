#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// P2996 reader for [[=halp::meta("key","value")]] annotations.

#include <avnd/common/meta_polyfill.hpp>

#if AVND_USE_STD_REFLECTION

#include <avnd/common/metadata_tag.hpp>

#include <string_view>

namespace avnd
{
// Value of the [[=halp::meta(key, ...)]] annotation on T, or "" if absent.
template <typename T>
consteval std::string_view annotated_metadata(std::string_view key) noexcept
{
  std::string_view result{};
  template for(constexpr std::meta::info a :
               std::define_static_array(std::meta::annotations_of(^^T)))
  {
    constexpr std::meta::info t = std::meta::remove_cv(std::meta::type_of(a));
    if constexpr(std::meta::has_template_arguments(t)
                 && std::meta::template_of(t) == ^^avnd::metadata_attribute)
    {
      constexpr auto value = std::meta::extract<typename[:t:]>(a);
      if(value.key() == key)
        result = std::define_static_string(value.value());
    }
  }
  return result;
}

template <typename T>
consteval bool has_annotated_metadata(std::string_view key) noexcept
{
  return !annotated_metadata<T>(key).empty();
}
}

#endif
