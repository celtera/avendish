#pragma once

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/common/arithmetic.hpp>
#if !defined(__cpp_lib_to_chars)
#include <boost/lexical_cast.hpp>
#else
#include <charconv>
#endif
#include <magic_enum.hpp>
#include <string>
#include <ext.h>

namespace max
{

// Note: this assumes that ac >= 1
struct from_atom
{
  t_atom& av;

  template<typename T>
    requires std::is_aggregate_v<T>
  bool operator()(T& v) const noexcept = delete;

  bool operator()(avnd::span_ish auto& v) const noexcept = delete;
  bool operator()(avnd::pair_ish auto& v) const noexcept = delete;
  bool operator()(avnd::tuple_ish auto& v) const noexcept = delete;
  bool operator()(avnd::map_ish auto& v) const noexcept = delete;
  bool operator()(avnd::set_ish auto& v) const noexcept = delete;
  bool operator()(avnd::bitset_ish auto& v) const noexcept = delete;

  template<typename T>
    requires std::is_enum_v<T>
  bool operator()(T& v) const noexcept
  {
    switch(av.a_type)
    {
      case A_LONG:
      {
        v = static_cast<T>(atom_getlong(&av));
        return true;
      }
      case A_FLOAT:
      {
        v = static_cast<T>(std::round(atom_getfloat(&av)));
        return true;
      }
      case A_SYM:
      {
        auto sym = atom_getsym(&av);
        if(sym && sym->s_name)
        {
          auto color = magic_enum::enum_cast<T>(sym->s_name);
          if (color.has_value()) {
            v = *color;
            return true;
          }
        }
        return false;
      }
      default:
        return false;
    }
  }

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
  template <std::size_t N>
  bool operator()(avnd::array_ish<N> auto&& v) const noexcept
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

  template<avnd::set_ish T>
  bool operator()(T& v) const noexcept
  {
    v.clear();

    using value_type = std::remove_cvref_t<typename T::value_type>;
    for(int i = 0; i < ac; i++)
    {
      value_type val;
      from_atom{av[i]}(val);
      v.insert(std::move(val));
    }
    return true;
  }

  template<avnd::map_ish T>
  bool operator()(T& v) const noexcept
  {
    v.clear();
    if(ac <= 1)
      return false;

    using value_type = std::remove_cvref_t<typename T::value_type>;
    using key_type = std::remove_cvref_t<typename T::key_type>;
    using mapped_type = std::remove_cvref_t<typename T::mapped_type>;
    for(int i = 0; i < ac / 2; i += 2)
    {
      key_type k;
      mapped_type val;
      from_atom{av[i]}(k);
      from_atom{av[i + 1]}(val);
      v.emplace(std::move(k), std::move(val));
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
    if(ac < 1)
      return false;
    return from_atom{av[0]}(v);
  }

  // FIXME
  template<avnd::tuple_ish T>
    requires (!avnd::pair_ish<T>)
  bool operator()(T& v) const noexcept { return false; }


  bool operator()(avnd::variant_ish auto& v) const noexcept
  {
    if(ac < 1)
      return false;
    // FIXME variant<std::vector<float>, std::vector<int>>
    switch(av[0].a_type)
    {
      case A_LONG:
      {
#define convert_long_to_type(t) \
        if constexpr(requires { v = (t)0; }) \
        { \
          v = (t)atom_getlong(&av[0]); \
          return true; \
        }
        avnd_for_all_integer_types(convert_long_to_type);
#undef convert_long_to_type
        break;
      }
      case A_FLOAT:
      {
#define convert_float_to_type(t) \
        if constexpr(requires { v = (t)0; }) \
          { \
                v = (t)atom_getfloat(&av[0]); \
                return true; \
          }
        avnd_for_all_fp_types(convert_float_to_type);
#undef convert_float_to_type
        break;
      }
      case A_SYM:
      {
        if constexpr(requires { v = "hello"; })
        {
          v = atom_getsym(&av[0])->s_name;
          return true;
        }
        break;
      }
    }

    return false;
  }

  template<avnd::optional_ish T>
  bool operator()(T& v) const noexcept
  {
    if(ac < 1)
      return false;

    typename T::value_type res;
    (*this)(res);
    v = std::move(res);
    return true;
  }

  template <typename T>
    requires(std::is_aggregate_v<T> && !avnd::span_ish<T> && avnd::pfr::tuple_size_v<T> > 0)
  bool operator()(T& v) const noexcept
  {
    avnd::for_each_field_ref(v, [this, i = 0] <typename F> (F& field) mutable {
      if(i < ac)
      {
        from_atom{av[i]}(field);
      }
    });
    return true;
  }

  bool operator()(avnd::bitset_ish auto& f)  noexcept
  {
    f.reset();
    for(int k = 0; k < std::min((int)f.size(), (int)ac); k++) {
      switch(av[k].a_type) {
        case A_LONG:
          if(av[k].a_w.w_long != 0)
            f.set(k);
          break;
        case A_FLOAT:
          if(av[k].a_w.w_long != 0.f)
            f.set(k);
          break;
        case A_SYM:
          if(av[k].a_w.w_sym != gensym(""))
            f.set(k);
          break;
        default:
          break;
      }
    }
    return true;
  }
};

}
