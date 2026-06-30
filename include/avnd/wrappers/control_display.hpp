#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/widechar.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/widgets.hpp>

#include <cstring>
#include <span>

#if __has_include(<fmt/format.h>) && !defined(AVND_DISABLE_FMT)
#include <fmt/format.h>
#else
#include <cstdlib>
#include <cstring>
#endif

namespace avnd
{
// FIXME refactor with vintage::Controls

template <typename C, typename T>
bool display_control(const T& value, char* cstr, std::size_t len)
{
  if constexpr(requires { C::display(std::span<char>(cstr, len), value); })
  {
    C::display(std::span<char>(cstr, len), value);
    return true;
  }
  else if constexpr(requires { C::display(cstr, value); })
  {
    C::display(cstr, value);
    return true;
  }
  else if constexpr(requires { C{value}.display(std::span<char>(cstr, len)); })
  {
    C{value}.display(std::span<char>(cstr, len));
    return true;
  }
  else if constexpr(requires { C{value}.display(cstr); })
  {
    C{value}.display(cstr);
    return true;
  }
  else if constexpr(requires { C{value}.display(); })
  {
    const std::string_view str = C{value}.display();
    int N = std::min(str.length() + 1, len - 1);
    const auto src = str.data();
    const auto dst = cstr;
    for(int i = 0; i < N; i++)
      dst[i] = src[i];
    return true;
  }
  else
  {
    using val_type = std::decay_t<decltype(value)>;

#if __has_include(<fmt/format.h>) && !defined(AVND_DISABLE_FMT)
    // Always reserve room for the NUL: fmt::format_to_n returns .out at
    // cstr + min(n, untruncated_size), so passing the full `len` and writing
    // *out would hit cstr[len] (one past the end) whenever the text reaches the
    // buffer size. Pass len-1 and terminate at the written end.
    // fmt::runtime: the format string is forwarded through this helper, so fmt's
    // consteval format-string check can't see it as a literal; these strings are
    // fixed and valid, so a runtime format string is fine.
    const auto put = [&](fmt::string_view f, const auto&... args) {
      if(len == 0)
        return;
      auto r = fmt::format_to_n(cstr, len - 1, fmt::runtime(f), args...);
      cstr[r.size < (len - 1) ? r.size : (len - 1)] = '\0';
    };
    if constexpr(std::floating_point<val_type>)
    {
      put("{:.2f}", value);
      return true;
    }
    else if constexpr(std::is_enum_v<val_type>)
    {
      static constexpr auto choices = avnd::get_enum_choices<C>();
      const int enum_index = static_cast<int>(value);
      if(enum_index >= 0 && enum_index < choices.size())
        put("{}", choices[enum_index]);
      else
        put("{}", enum_index);
      return true;
    }
    else if constexpr(avnd::optional_ish<T>)
    {
      using inner_type = std::decay_t<decltype(*value)>;
      if constexpr(fmt::is_formattable<inner_type>::value)
      {
        if(value)
          put("{}", *value);
        else
          put("(nullopt)");
        return true;
      }
      else
      {
        // No fmt formatter for the wrapped type (e.g. halp::impulse_type):
        // nothing printable, report empty rather than failing to compile.
        if(len > 0)
          cstr[0] = '\0';
        return false;
      }
    }
    else if constexpr(fmt::is_formattable<val_type>::value)
    {
      put("{}", value);
      return true;
    }
    else
    {
      // Value types fmt cannot format on its own (colors, vectors, xy/xyz/xyzw,
      // range sliders, tensors, ...). These have no textual control display;
      // degrade gracefully instead of breaking the build.
      if(len > 0)
        cstr[0] = '\0';
      return false;
    }
#else
    if constexpr(std::floating_point<val_type>)
    {
      snprintf(cstr, 16, "%.2f", value);
      return true;
    }
    else if constexpr(std::is_same_v<val_type, int>)
    {
      snprintf(cstr, 16, "%d", value);
      return true;
    }
    else if constexpr(std::is_same_v<val_type, bool>)
    {
      snprintf(cstr, 16, value ? "true" : "false");
      return true;
    }
    else if constexpr(std::is_same_v<val_type, const char*>)
    {
      snprintf(cstr, 16, "%s", value);
      return true;
    }
    else if constexpr(std::is_same_v<val_type, std::string>)
    {
      snprintf(cstr, 16, "%s", value.data());
      return true;
    }
    else if constexpr(std::is_enum_v<val_type>)
    {
      static constexpr auto choices = avnd::get_enum_choices<C>();
      const int enum_index = static_cast<int>(value);
      if(enum_index >= 0 && enum_index < choices.size())
        snprintf(cstr, 16, "%s", choices[enum_index].data());
      else
        snprintf(cstr, 16, "%d", enum_index);
      return true;
    }
#endif
    return false;
  }
}
template <typename C, typename T>
bool display_control(const T& value, char16_t* string, std::size_t sz)
{
  char temp[512] = {0};
  if(display_control<C>(value, temp, sz))
  {
    utf8_to_utf16(temp, temp + strlen(temp), string);
    return true;
  }
  return false;
}
template <typename C, typename T>
bool display_control(const T& value, wchar_t* string, std::size_t sz)
{
  char temp[512] = {0};
  if(display_control<C>(value, temp, sz))
  {
    utf8_to_utf16(temp, temp + strlen(temp), string);
    return true;
  }
  return false;
}
}
