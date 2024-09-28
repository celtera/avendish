#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/coroutines.hpp>
#include <avnd/concepts/generic.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<boost/core/demangle.hpp>)
#include <boost/core/demangle.hpp>
#endif

namespace avnd
{
#if __cpp_if_consteval >= 202106L
#define AVND_IF_CONSTEVAL consteval
#else
#define AVND_IF_CONSTEVAL (std::is_constant_evaluated())
#endif

#define define_get_property(PropName, Type, Default)                                  \
  template <typename T>                                                               \
  constexpr Type get_##PropName()                                                     \
  {                                                                                   \
    if constexpr(requires { Type{T::PropName()}; })                                   \
      return T::PropName();                                                           \
    else if constexpr(requires { Type{T::PropName}; })                                \
      return T::PropName;                                                             \
    else                                                                              \
      return Default;                                                                 \
  }                                                                                   \
                                                                                      \
  template <typename T>                                                               \
  constexpr Type get_##PropName(const T& t)                                           \
  {                                                                                   \
    if AVND_IF_CONSTEVAL                                                              \
    {                                                                                 \
      return get_##PropName<T>();                                                     \
    }                                                                                 \
    else                                                                              \
    {                                                                                 \
      if constexpr(requires { Type{T::PropName()}; })                                 \
        return T::PropName();                                                         \
      else if constexpr(requires { Type{T::PropName}; })                              \
        return T::PropName;                                                           \
      else if constexpr(requires { Type{t.PropName()}; })                             \
        return t.PropName();                                                          \
      else if constexpr(requires { Type{t.PropName}; })                               \
        return t.PropName;                                                            \
      else                                                                            \
        return get_##PropName<T>();                                                   \
    }                                                                                 \
  }                                                                                   \
                                                                                      \
  template <typename T>                                                               \
  concept has_##PropName                                                              \
      = requires(T t) { Type{t.PropName()}; } || requires(T t) { Type{t.PropName}; }; \
                                                                                      \
  struct prop_##PropName                                                              \
  {                                                                                   \
    template <typename T>                                                             \
    static constexpr bool has() noexcept                                              \
    {                                                                                 \
      return has_##PropName<T>;                                                       \
    }                                                                                 \
                                                                                      \
    static constexpr std::string_view name() noexcept                                 \
    {                                                                                 \
      return #PropName;                                                               \
    }                                                                                 \
                                                                                      \
    template <typename T>                                                             \
    static constexpr Type get()                                                       \
    {                                                                                 \
      if constexpr(has_##PropName<T>)                                                 \
        return get_##PropName<T>();                                                   \
      else                                                                            \
        return Default;                                                               \
    }                                                                                 \
  };

#if __has_include(<boost/core/demangle.hpp>) && !defined(BOOST_NO_RTTI)
template <typename T>
std::string_view get_typeid_name()
{
  static const auto str = []() -> std::string {
    auto dem = boost::core::demangle(typeid(T).name());
    auto idx = dem.find_last_of(':');
    if(idx != std::string::npos)
      return dem.substr(idx + 1);
    else
      return dem;
  }();
  return str;
}
#else
template <typename T>
constexpr std::string_view get_typeid_name()
{
  return "(name)";
}
#endif

// Public-facing display name of objects, ports, etc.
define_get_property(name, std::string_view, get_typeid_name<T>())

// Name used for interoperability, message-passing, etc. No spaces or special characters except underscore.
define_get_property(c_name, std::string_view, "(c name)")

// Define an explicit optional symbol for messages in Max / Pd
define_get_property(symbol, std::string_view, "(symbol)")

// Uniquely identifies an object
define_get_property(uuid, std::string_view, "00000000-0000-0000-0000-000000000000")

// Multiple common product metadatas
define_get_property(vendor, std::string_view, "(vendor)")
define_get_property(product, std::string_view, "(product)")
define_get_property(version, std::string_view, "(version)")
define_get_property(label, std::string_view, "(label)")
define_get_property(short_label, std::string_view, "(short label)")
define_get_property(category, std::string_view, "(category)")
define_get_property(copyright, std::string_view, "(copyright)")
define_get_property(license, std::string_view, "(license)")
define_get_property(url, std::string_view, "(url)")
define_get_property(email, std::string_view, "(email)")
define_get_property(manual_url, std::string_view, "(manual_url)")
define_get_property(support_url, std::string_view, "(support_url)")
define_get_property(description, std::string_view, "(description)")
define_get_property(short_description, std::string_view, "(short_description)")
define_get_property(module, std::string_view, "(module)")

template <typename T>
constexpr std::string_view default_osc_path()
{
  return get_name<T>();
}
define_get_property(path, std::string_view, default_osc_path<T>())

// FIXME: C++23: constexpr ?
std::string array_to_string(auto& authors)
{
  std::string ret;
  for(int i = 0; i < std::ssize(authors); i++)
  {
    ret += authors[i];
    if(i < std::ssize(authors) - 1)
    {
      ret += ", ";
    }
  }
  return ret;
}

static constexpr bool valid_char_for_c_identifier(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
         || (c == '_');
}

template<auto Symtab>
static constexpr bool validate_name(std::string_view name)
{
  // FIXME we could replace with underscore with some compile-time string library
  for(char c : name)
    if(!Symtab(c))
      return false;
  return true;
}

template<std::size_t Sz>
struct static_identifier: std::array<char, Sz + 1>
{
// operator std::string_view() const noexcept {
//   return std::string_view{data(), Sz};
// }
  constexpr bool operator==(std::string_view other) const noexcept {
    return std::string_view{this->data(), Sz} == other;
  }
};

template <typename T, auto Symtab = valid_char_for_c_identifier>
static constexpr auto fixup_identifier()
{
  constexpr auto nm = avnd::get_name<T>();
  constexpr auto sz = nm.size();
  static_identifier<sz> storage;
  for(std::size_t i = 0; i < sz; i++)
    storage[i] = Symtab(nm[i]) ? nm[i] : '_';
  storage[sz] = 0;
  return storage;
}

template <typename T, auto Symtab = valid_char_for_c_identifier>
static constexpr auto get_c_identifier()
{
  if constexpr(avnd::has_c_name<T>)
  {
    return avnd::get_c_name<T>();
  }
  else if constexpr(avnd::has_name<T>)
  {
    constexpr auto name = avnd::get_name<T>();
    if constexpr(validate_name<Symtab>(name))
    {
      return name;
    }
    else
    {
      return fixup_identifier<T, Symtab>();
    }
  }
  else
  {
    using namespace std::literals;
    // FIXME variable name with pfr
    return "unnamed"sv;
  }
}

template <typename T, auto Symtab = valid_char_for_c_identifier>
static constexpr auto get_static_symbol()
{
  if constexpr(avnd::has_symbol<T>)
    return avnd::get_symbol<T>();
  else
    return avnd::get_c_identifier<T, Symtab>();
}



template <typename T>
concept has_author
    = requires { std::string_view{T::author()}; }
      || requires { std::string_view{T::author}; } || requires { T::authors(); }
      || requires { T::authors; } || requires { std::string_view{T::developer()}; }
      || requires { std::string_view{T::developer}; } || requires { T::developers(); }
      || requires { T::developers; } || requires { std::string_view{T::creator()}; }
      || requires { std::string_view{T::creator}; } || requires { T::creators(); }
      || requires { T::creators; };
template <typename T>
/* constexpr */ auto get_author()
{
  if constexpr(requires { T::author(); })
    return T::author();
  else if constexpr(requires { T::author; })
    return T::author;
  else if constexpr(requires { T::authors(); })
    return array_to_string(T::authors());
  else if constexpr(requires { T::authors; })
    return array_to_string(T::authors);
  else if constexpr(requires { T::developer(); })
    return T::developer();
  else if constexpr(requires { T::developer; })
    return T::developer;
  else if constexpr(requires { T::developers(); })
    return array_to_string(T::developers());
  else if constexpr(requires { T::developers; })
    return array_to_string(T::developers);
  else if constexpr(requires { T::creator(); })
    return T::creator();
  else if constexpr(requires { T::creator; })
    return T::creator;
  else if constexpr(requires { T::creators(); })
    return array_to_string(T::creators());
  else if constexpr(requires { T::creators; })
    return array_to_string(T::creators);
  else
    return "(author)";
}

template <typename T>
concept has_unit = requires { std::string_view{T::unit()}; }
                   || requires { std::string_view{T::unit}; }
                   || requires { std::string_view{T::dataspace()}; }
                   || requires { std::string_view{T::dataspace}; }
                   || requires { std::string_view{T::type()}; }
                   || requires { std::string_view{T::type}; };

template <typename T>
constexpr std::string_view get_unit()
{
  if constexpr(requires { T::unit(); })
    return T::unit();
  else if constexpr(requires { T::unit; })
    return T::unit;
  else if constexpr(requires { T::dataspace(); })
    return T::dataspace();
  else if constexpr(requires { T::dataspace; })
    return T::dataspace;
  else if constexpr(requires { T::type(); })
    return T::type();
  else if constexpr(requires { T::type; })
    return T::type;
  else
    return "";
}

template <typename T>
/* constexpr */ int get_int_version()
{
  if constexpr(requires {
                 {
                   T::version()
                 } -> std::integral;
               })
  {
    return T::version();
  }
  else if constexpr(requires {
                      {
                        std::declval<T>().version
                      } -> std::integral;
                    })
  {
    return T::version;
  }
  else if constexpr(requires {
                      {
                        T::version()
                        } -> avnd::string_ish;
                    } || requires {
             {
               std::declval<T>().version
               } -> avnd::string_ish;
           })
  {
    auto str = avnd::get_version<T>();
    if(str.empty())
      return 0;

    const char* nptr = str.data();
    char* endptr[1]{};
    int ret = std::strtol(nptr, endptr, 0);
    if(*endptr == nptr)
      return 0;

    return ret;
  }
  else
  {
    return 1;
  }
}
template <typename T, char Sep>
constexpr std::array<char, 256> get_keywords()
{
  if constexpr(requires { T::keywords(); })
  {
    const auto& w = T::keywords();
    constexpr auto n = std::ssize(w);
    if(n > 0)
    {
      constexpr int cmax = 255;
      std::array<char, 256> kw;
      kw.fill(0);

      int c = 0;
      for(int i = 0; i < n; i++)
      {
        for(char ch : w[i])
        {
          if(c >= cmax)
            break;
          kw[c++] = ch;
        }
        if(c >= cmax)
          break;

        if(i < n - 1)
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
  if constexpr(requires { T::tags(); })
    for(std::string_view tag : T::tags())
      co_yield tag;
  else if constexpr(requires { T::tags; })
    for(std::string_view tag : T::tags)
      co_yield tag;
  else
    co_return;
}
#else
template <typename T>
inline std::vector<std::string_view> get_tags()
{
  if constexpr(requires { T::tags(); })
  {
    constexpr auto t = T::tags();
    return std::vector<std::string_view>(std::begin(t), std::end(t));
  }
  else if constexpr(requires { T::tags; })
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
