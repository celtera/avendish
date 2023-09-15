#pragma once

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/aggregates.hpp>
#if !defined(__cpp_lib_to_chars)
#include <boost/lexical_cast.hpp>
#else
#include <charconv>
#endif
#include <string>
#include <ext.h>

namespace max
{

// Note: this assumes that ac >=
struct from_atom
{
  t_atom& av;
  template<typename T>
    requires std::integral<T> || std::floating_point<T>
  bool operator()(T& v) const noexcept
  {
    switch(av.a_type)
    {
      case A_LONG:
      {
        v = atom_getlong(&av);
        return true;
      }
      case A_FLOAT:
      {
        v = atom_getfloat(&av);
        return true;
      }
      case A_SYM:
      {
        auto sym = atom_getsym(&av);
        if(sym && sym->s_name)
        {
          double vv{};
          #if defined(__cpp_lib_to_chars)
          auto [_, ec] = std::from_chars(sym->s_name, sym->s_name + strlen(sym->s_name), vv);
          if(ec == std::errc{})
          {
            v = vv;
            return true;
          }
          #else
          std::string_view str{sym->s_name, sym->s_name + strlen(sym->s_name)};
          if(boost::conversion::try_lexical_convert(str, vv))
          {
            v = vv;
            return true;
          }
          #endif
        }
        return false;
      }
    }

    return false;
  }

  bool operator()(std::string& v) const noexcept
  {
    if(av.a_type == A_SYM)
    {
      if(auto sym = atom_getsym(&av); sym && sym->s_name)
      {
        v = sym->s_name;
      }
    }
    return true;
  }
};

struct from_atoms
{
  long ac{};
  t_atom* av{};

  bool operator()(std::integral auto& v) const noexcept
  {
    return from_atom{av[0]}(v);
  }

  bool operator()(std::floating_point auto& v) const noexcept
  {
    return from_atom{av[0]}(v);
  }

  template<typename T>
    requires std::is_enum_v<T>
  bool operator()(T& v) const noexcept
  {
    auto r = std::to_underlying(v);
    auto res = from_atom{av[0]}(r);
    if(res)
      v = static_cast<T>(r);
    return res;
  }

  template<typename T>
    requires (std::is_class_v<T> && avnd::pfr::tuple_size_v<T> == 0)
  bool operator()(T& v) const noexcept
  {
    // Impulse case, nothing to do
    // FIXME unless we have an optional parameter !
    return true;
  }

  bool operator()(std::string& v) const noexcept
  {
    return from_atom{av[0]}(v);
  }
};

}
