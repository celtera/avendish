#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/coroutines.hpp>

#include <array>
#include <string>
#include <vector>

#include <string_view>

namespace avnd
{
#define define_get_property(PropName, Type, Default)    \
  template <typename T>                                 \
  constexpr Type get_##PropName()                       \
  {                                                     \
    if constexpr (requires { Type{T::PropName()}; })    \
      return T::PropName();                             \
    else if constexpr (requires { Type{T::PropName}; }) \
      return T::PropName;                               \
    else                                                \
      return Default;                                   \
  }                                                     \
                                                        \
  template <typename T>                                 \
  constexpr Type get_##PropName(const T&)               \
  {                                                     \
    return get_##PropName<T>();                         \
} \
\
template<typename T> \
concept has_##PropName = \
   requires { Type{T::PropName()}; } \
|| requires { Type{T::PropName}; };

// clang-format: off
define_get_property(name, std::string_view, "(name)")
define_get_property(c_name, std::string_view, "(c name)")
define_get_property(vendor, std::string_view, "(vendor)")
define_get_property(product, std::string_view, "(product)")
define_get_property(version, std::string_view, "(version)")
define_get_property(label, std::string_view, "(label)")
define_get_property(uuid, std::string_view, "00000000-0000-0000-0000-000000000000")
define_get_property(short_label, std::string_view, "(short label)")
define_get_property(category, std::string_view, "(category)")
define_get_property(copyright, std::string_view, "(copyright)")
define_get_property(license, std::string_view, "(license)")
define_get_property(url, std::string_view, "(url)")
define_get_property(email, std::string_view, "(email)")
define_get_property(manual_url, std::string_view, "(manual_url)")
define_get_property(support_url, std::string_view, "(support_url)")
define_get_property(description, std::string_view, "(description)")
// clang-format: on

// FIXME: C++23: constexpr ?
std::string array_to_string(auto& authors)
{
  std::string ret;
  for (int i = 0; i < std::ssize(authors); i++)
  {
    ret += authors[i];
    if (i < std::ssize(authors) - 1)
    {
      ret += ", ";
    }
  }
  return ret;
}

template <typename T>
/* constexpr */ auto get_author()
{
  if constexpr (requires { T::author(); })
    return T::author();
  else if constexpr (requires { T::author; })
    return T::author;
  else if constexpr (requires { T::authors(); })
    return array_to_string(T::authors());
  else if constexpr (requires { T::authors; })
    return array_to_string(T::authors);
  else if constexpr (requires { T::developer(); })
    return T::developer();
  else if constexpr (requires { T::developer; })
    return T::developer;
  else if constexpr (requires { T::developers(); })
    return array_to_string(T::developers());
  else if constexpr (requires { T::developers; })
    return array_to_string(T::developers);
  else if constexpr (requires { T::creator(); })
    return T::creator();
  else if constexpr (requires { T::creator; })
    return T::creator;
  else if constexpr (requires { T::creators(); })
    return array_to_string(T::creators());
  else if constexpr (requires { T::creators; })
    return array_to_string(T::creators);
  else
    return "(author)";
}

template<typename T>
concept has_unit =
   requires { std::string_view{T::unit()}; }
|| requires { std::string_view{T::unit}; }
|| requires { std::string_view{T::dataspace()}; }
|| requires { std::string_view{T::dataspace}; }
|| requires { std::string_view{T::type()}; }
|| requires { std::string_view{T::type}; }
;

template <typename T>
/* constexpr */ auto get_unit()
{
  if constexpr (requires { T::unit(); })
    return T::unit();
  else if constexpr (requires { T::unit; })
    return T::unit;
  else if constexpr (requires { T::dataspace(); })
    return T::dataspace();
  else if constexpr (requires { T::dataspace; })
    return T::dataspace;
  else if constexpr (requires { T::type(); })
    return T::type();
  else if constexpr (requires { T::type; })
    return T::type;
  else
    return "";
}

template <typename T, char Sep>
constexpr std::array<char, 256> get_keywords()
{
  if constexpr (requires { T::keywords(); })
  {
    const auto& w = T::keywords();
    constexpr auto n = std::ssize(w);
    if (n > 0)
    {
      constexpr int cmax = 255;
      std::array<char, 256> kw;
      kw.fill(0);

      int c = 0;
      for (int i = 0; i < n; i++)
      {
        for (char ch : w[i])
        {
          if (c >= cmax)
            break;
          kw[c++] = ch;
        }
        if (c >= cmax)
          break;

        if (i < n - 1)
          kw[c++] = Sep;
      }

      return kw;
    }
  }

  return {};
}

#if !AVND_DISABLE_COROUTINES
using tag_iterator = avnd::generator<std::string_view>;
template <typename T>
inline tag_iterator get_tags()
{
  if constexpr (requires { T::tags(); })
    for (std::string_view tag : T::tags())
      co_yield tag;
  else if constexpr (requires { T::tags; })
    for (std::string_view tag : T::tags)
      co_yield tag;
}
#else
template <typename T>
inline std::vector<std::string_view> get_tags()
{
  if constexpr (requires { T::tags(); })
  {
    constexpr auto t = T::tags();
    return std::vector<std::string_view>(std::begin(t), std::end(t));
  }
  else if constexpr (requires { T::tags; })
  {
    return std::vector<std::string_view>(std::begin(T::tags), std::end(T::tags));
  }
  else
  {
    return {};
  }
}
#endif
}
