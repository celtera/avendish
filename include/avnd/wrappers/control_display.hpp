#pragma once
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/widechar.hpp>
#include <cstring>
#include <algorithm>

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
#else
#include <cstdlib>
#include <cstring>
#endif


namespace avnd
{
// FIXME refactor with vintage::Controls

template<typename C, typename T>
bool display_control(const T& value, char* cstr)
{
  if constexpr(requires { C::display(cstr, value); })
  {
    C::display(cstr, value);
    return true;
  }
  else if constexpr(requires { C{value}.display(cstr); })
  {
    C{value}.display(cstr);
    return true;
  }
  else if constexpr(requires { C{value}.display(); })
  {
    auto str = C{value}.display();
    std::copy_n(str, strlen(str) + 1, cstr);
    return true;
  }
  else
  {
    using val_type = std::decay_t<decltype(value)>;

#if __has_include(<fmt/format.h>)
    if constexpr (std::floating_point<val_type>)
    { fmt::format_to(cstr, "{:.2f}", value); }
    else if constexpr (std::is_enum_v<val_type>)
    {
      const int enum_index = static_cast<int>(value);
      if(enum_index >= 0 && enum_index < C::choices().size())
        fmt::format_to(cstr, "{}", C::choices()[enum_index]);
      else
        fmt::format_to(cstr, "{}", enum_index);
    }
    else
    { fmt::format_to(cstr, "{}", value); }
#else
    if constexpr (std::floating_point<val_type>)
        snprintf(cstr, 16, "%.2f", value);
    else if constexpr (std::is_same_v<val_type, int>)
        snprintf(cstr, 16, "%d", value);
    else if constexpr (std::is_same_v<val_type, bool>)
        snprintf(cstr, 16, value ? "true" : "false");
    else if constexpr (std::is_same_v<val_type, const char*>)
        snprintf(cstr, 16, "%s", value);
    else if constexpr (std::is_same_v<val_type, std::string>)
        snprintf(cstr, 16, "%s", value.data());
    else if constexpr (std::is_enum_v<val_type>)
    {
      const int enum_index = static_cast<int>(value);
      if(enum_index >= 0 && enum_index < C::choices().size())
        snprintf(cstr, 16, "%s", C::choices()[enum_index].data());
      else
        snprintf(cstr, 16, "%d", enum_index);
    }
#endif
    return false;
  }
}
template<typename C, typename T>
bool display_control(const T& value, char16_t* string)
{
  char temp[512];
  if(display_control<C>(value, temp))
  {
    utf8_to_utf16(temp, temp + strlen(temp), string);
    return true;
  }
  return false;
}
template<typename C, typename T>
bool display_control(const T& value, wchar_t* string)
{
  char temp[512];
  if(display_control<C>(value, temp))
  {
    utf8_to_utf16(temp, temp + strlen(temp), string);
    return true;
  }
  return false;
}
}
