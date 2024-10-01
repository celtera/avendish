#pragma once
#include <avnd/binding/pd/helpers.hpp>
#include <avnd/common/arithmetic.hpp>
#include <magic_enum.hpp>

#if !defined(__cpp_lib_to_chars)
#include <boost/lexical_cast.hpp>
#else
#include <charconv>
#endif

#include <cstring>
#include <cmath>
#include <mutex>
namespace pd
{

// Note: this assumes that ac >=
struct from_atom
{
  t_atom& av;

  template<typename T>
    requires std::is_aggregate_v<T>
  bool operator()(T& v) const noexcept = delete;

  bool operator()(avnd::iterable_ish auto& v) const noexcept = delete;
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
      case A_FLOAT:
      {
        v = static_cast<T>(std::round(av.a_w.w_float));
        return true;
      }
      case A_SYMBOL:
      {
        auto sym = av.a_w.w_symbol;
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
    switch(av.a_type)
    {
      case A_FLOAT:
      {
        // FIXME to_chars?
        v = std::to_string(av.a_w.w_float);
        break;
      }
      case A_SYMBOL:
      {
        if(auto sym = av.a_w.w_symbol; sym && sym->s_name)
        {
          v = sym->s_name;
        }
        break;
      }
    }
    return true;
  }
};

struct from_atoms
{
  long ac{};
  t_atom* av{};

  bool operator()(std::integral auto& v) const noexcept {
    if(ac < 1)
      return false;
    return from_atom{av[0]}(v); }

  bool operator()(std::floating_point auto& v) const noexcept
  {
    if(ac < 1)
      return false;
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

  template <avnd::list_ish T>
  bool operator()(T& v) const noexcept
  {
    v.clear();

    for(int i = 0; i < ac; i++)
    {
      typename T::value_type item;
      from_atom{av[i]}(item);
      v.push_back(std::move(item));
    }
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

  template <typename T>
    requires std::is_enum_v<T>
  bool operator()(T& v) const noexcept
  {
    if(ac < 1)
      return false;

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

  bool operator()(std::string& v) const noexcept {
    if(ac < 1)
      return false;
    return from_atom{av[0]}(v);
  }

  template <avnd::tuple_ish T>
    requires(!avnd::pair_ish<T>)
  bool operator()(T& v) const noexcept
  {
    static constexpr int N = std::tuple_size_v<T>;
    if(ac < N)
      return false;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (from_atom{av[I]}(std::get<I>(v)), ...);
    }(std::make_index_sequence<N>{});

    return true;
  }

  bool operator()(avnd::variant_ish auto& v) const noexcept
  {
    if(ac < 1)
      return false;
    // FIXME variant<std::vector<float>, std::vector<int>>
    switch(av[0].a_type)
    {
      case A_FLOAT:
      {
#define convert_float_to_type(t) \
        if constexpr(requires { v = (t)0; }) \
          { \
                v = (t)av[0].a_w.w_float; \
                return true; \
          }
        avnd_for_all_fp_types(convert_float_to_type);
#undef convert_float_to_type
        break;
      }
      case A_SYMBOL:
      {
        if constexpr(requires { v = "hello"; })
        {
          v = av[0].a_w.w_symbol->s_name;
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
    requires(
        std::is_aggregate_v<T> && !avnd::iterable_ish<T>
        && avnd::pfr::tuple_size_v<T> > 0)
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
        case A_FLOAT:
          if(av[k].a_w.w_float != 0.f)
            f.set(k);
          break;
        case A_SYMBOL:
          if(av[k].a_w.w_symbol != gensym(""))
            f.set(k);
          break;
        default:
          break;
      }
    }
    return true;
  }
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
          // If we get a message [port_sym 1 2 3)
          auto port_sym = pd::symbol_for_port<M>();

          // Then the message through this inlet will be received by "process"
          // as [name 1 2 3)
          auto name = pd::symbol_from_name<M>();

          inlet_new(&x_obj, &x_obj.ob_pd, port_sym, name);
        }
      }
    });
  }

  template <typename Field>
  bool
  process_inlet_control(T& obj, Field& field, int argc, t_atom* argv)
  {
    if constexpr(convertible_to_atom_list_statically<decltype(field.value)>)
    {
      if(from_atoms{argc, argv}(field.value))
      {
        if_possible(field.update(obj));
        return true;
      }
    }
    else
    {
      static std::once_flag f;
      std::call_once(f, [] {
        post("Field type not supported as input: %s",
#if defined(_MSC_VER)
             __FUNCSIG__
#else
             __PRETTY_FUNCTION__
#endif
        );
      });
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

      for(auto state : implementation.full_state())
      {
        avnd::parameter_input_introspection<T>::for_all(
            state.inputs, [&,&obj=state.effect]<typename M>(M& field) {
          if(ok)
            return;
          if(symname == pd::get_name_symbol<M>())
          {
            ok = process_inlet_control(obj, field, argc, argv);
          }
        });
      }
      return ok;
    }
    return false;
  }
};

}
