#pragma once
#include <avnd/binding/pd/helpers.hpp>

#if !defined(__cpp_lib_to_chars)
#include <boost/lexical_cast.hpp>
#else
#include <charconv>
#endif

#include <cstring>
namespace pd
{

// Note: this assumes that ac >=
struct from_atom
{
  t_atom& av;
  template <typename T>
    requires std::integral<T> || std::floating_point<T>
  bool operator()(T& v) const noexcept
  {
    switch(av.a_type)
    {
      case A_FLOAT: {
        v = atom_getfloat(&av);
        return true;
      }
      case A_SYMBOL: {
        auto sym = av.a_w.w_symbol;
        if(sym && sym->s_name)
        {
          double vv{};
#if defined(__cpp_lib_to_chars)
          auto [_, ec]
              = std::from_chars(sym->s_name, sym->s_name + strlen(sym->s_name), vv);
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
    if(av.a_type == A_SYMBOL)
    {
      if(auto sym = av.a_w.w_symbol; sym && sym->s_name)
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

  bool operator()(std::integral auto& v) const noexcept { return from_atom{av[0]}(v); }

  bool operator()(std::floating_point auto& v) const noexcept
  {
    return from_atom{av[0]}(v);
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

  template <typename T>
    requires std::is_enum_v<T>
  bool operator()(T& v) const noexcept
  {
    auto r = static_cast<std::underlying_type_t<T>>(v);
    auto res = from_atom{av[0]}(r);
    if(res)
      v = static_cast<T>(r);
    return res;
  }

  template <typename T>
    requires(
        std::is_class_v<T> && avnd::pfr::tuple_size_v<T> == 0
        && std::is_trivial_v<T> && std::is_standard_layout_v<T>)
  bool operator()(T& v) const noexcept
  {
    // Impulse case, nothing to do
    // FIXME unless we have an optional parameter !
    return true;
  }

  bool operator()(std::string& v) const noexcept { return from_atom{av[0]}(v); }
};

template <typename T>
struct inputs
{
  void init(avnd::effect_container<T>& implementation, t_object& x_obj)
  {
    int k = 0;
    avnd::input_introspection<T>::for_all(
        avnd::get_inputs<T>(implementation), [&x_obj, &k]<typename M>(M& ctl) {
      // Skip the first port
      if(k++)
      {
        // Do not create a port for attributes
        if constexpr(!avnd::attribute_port<M>)
        {
          static const auto name = pd::symbol_from_name<M>();

          inlet_new(&x_obj, &x_obj.ob_pd, pd::symbol_for_port<M>(), name);
        }
      }
    });
  }

  template <typename Field>
  bool
  process_inlet_control(T& obj, Field& field, int argc, t_atom* argv)
  {
    if(from_atoms{argc, argv}(field.value))
    {
      if_possible(field.update(obj));
      return true;
    }
    return false;
  }

  bool process_inputs(
      avnd::effect_container<T>& implementation, t_symbol* s, int argc, t_atom* argv)
  {
    // FIXME create static pointer tables instead
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      bool ok = false;
      std::string_view symname = s->s_name;
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(implementation), [&]<typename M>(M& field) {
        if(ok)
          return;
        if(symname == pd::get_name_symbol<M>())
        {
          ok = process_inlet_control(implementation.effect, field, argc, argv);
        }
      });
      return ok;
    }
    return false;
  }
};

}
