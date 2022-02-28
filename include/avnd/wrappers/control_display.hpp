#pragma once
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/widechar.hpp>

#include <cstring>

#if __has_include(<fmt/format.h>)
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
  if constexpr (requires { C::display(cstr, value); })
  {
    C::display(cstr, value);
    return true;
  }
  else if constexpr (requires { C{value}.display(cstr); })
  {
    C{value}.display(cstr);
    return true;
  }
  else if constexpr (requires { C{value}.display(); })
  {
    const std::string_view str = C{value}.display();
    int N = std::min(str.length() + 1, len - 1);
    const auto src = str.data();
    const auto dst = cstr;
    for (int i = 0; i < N; i++)
      dst[i] = src[i];
    return true;
  }
  else
  {
    using val_type = std::decay_t<decltype(value)>;

#if __has_include(<fmt/format.h>)
    if constexpr (std::floating_point<val_type>)
    {
      *fmt::format_to_n(cstr, len, "{:.2f}", value).out = '\0';
      return true;
    }
    else if constexpr (std::is_enum_v<val_type>)
    {
      const int enum_index = static_cast<int>(value);
      if (enum_index >= 0 && enum_index < C::choices().size())
        *fmt::format_to_n(cstr, len, "{}", C::choices()[enum_index]).out = '\0';
      else
        *fmt::format_to_n(cstr, len, "{}", enum_index).out = '\0';
      return true;
    }
    else
    {
      *fmt::format_to_n(cstr, len, "{}", value).out = '\0';
      return true;
    }
#else
    if constexpr (std::floating_point<val_type>)
    {
      snprintf(cstr, 16, "%.2f", value);
      return true;
    }
    else if constexpr (std::is_same_v<val_type, int>)
    {
      snprintf(cstr, 16, "%d", value);
      return true;
    }
    else if constexpr (std::is_same_v<val_type, bool>)
    {
      snprintf(cstr, 16, value ? "true" : "false");
      return true;
    }
    else if constexpr (std::is_same_v<val_type, const char*>)
    {
      snprintf(cstr, 16, "%s", value);
      return true;
    }
    else if constexpr (std::is_same_v<val_type, std::string>)
    {
      snprintf(cstr, 16, "%s", value.data());
      return true;
    }
    else if constexpr (std::is_enum_v<val_type>)
    {
      const int enum_index = static_cast<int>(value);
      if (enum_index >= 0 && enum_index < C::choices().size())
        snprintf(cstr, 16, "%s", C::choices()[enum_index].data());
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
  if (display_control<C>(value, temp, sz))
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
  if (display_control<C>(value, temp, sz))
  {
    utf8_to_utf16(temp, temp + strlen(temp), string);
    return true;
  }
  return false;
}
}
