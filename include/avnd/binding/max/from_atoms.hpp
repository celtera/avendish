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
          std::string_view str{sym->s_name, strlen(sym->s_name)};
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
    if(ac >= 0)
      return from_atom{av[0]}(v);
    return false;
  }

  bool operator()(std::floating_point auto& v) const noexcept
  {
    if(ac >= 0)
      return from_atom{av[0]}(v);
    return false;
  }

  template<typename T>
    requires std::is_enum_v<T>
  bool operator()(T& v) const noexcept
  {
    if(ac == 0)
      return false;
    auto r = static_cast<std::underlying_type_t<T>>(v);
    auto res = from_atom{av[0]}(r);
    if(res)
      v = static_cast<T>(r);
    return res;
  }

  bool operator()(avnd::vector_ish auto& v) const noexcept
  {
    v.clear();
    v.resize(ac);

    for(int i = 0; i < ac; i++)
      from_atom{av[i]}(v[i]);
    return true;
  }

  template <std::size_t N>
  bool operator()(avnd::array_ish<N> auto& v) const noexcept
  {
    for(int i = 0; i < ac; i++)
      from_atom{av[i]}(v[i]);
    return true;
  }

  bool operator()(avnd::pair_ish auto& v) const noexcept
  {
    if(ac < 2)
      return false;

    from_atom{av[0]}(v.first);
    from_atom{av[1]}(v.second);
    return true;
  }

  bool operator()(avnd::set_ish auto& v) const noexcept
  {
    v.clear();

    using value_type = std::remove_cvref_t<typename decltype(v)::value_type>;
    for(int i = 0; i < ac; i++)
    {
      value_type val;
      from_atom{av[i]}(val);
      v.insert(std::move(val));
    }
    return true;
  }

  bool operator()(avnd::map_ish auto& v) const noexcept
  {
    // FIXME handle dict inputs
    v.clear();
    if(ac <= 1)
      return false;

    using value_type = std::remove_cvref_t<typename decltype(v)::value_type>;
    using key_type = std::remove_cvref_t<typename decltype(v)::key_type>;
    using mapped_type = std::remove_cvref_t<typename decltype(v)::mapped_type>;
    for(int i = 0; i < ac / 2; i += 2)
    {
      value_type val;
      from_atom{av[i]}(val.first);
      from_atom{av[i + 1]}(val.second);
      v.insert(std::move(val));
    }
    return true;
  }

  template<typename T>
    requires (std::is_class_v<T> && avnd::pfr::tuple_size_v<T> == 0
             && std::is_trivial_v<T> && std::is_standard_layout_v<T>)
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
