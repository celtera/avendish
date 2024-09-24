#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/to_atoms.hpp>
#include <commonsyms.h>
namespace max
{
struct do_value_to_max_typed
{
  t_outlet* p;

  void operator()() const noexcept
  {
    outlet_bang(p);
  }
  void operator()(std::floating_point auto v) const noexcept
  {
    outlet_float(p, v);
  }
  void operator()(std::integral auto v) const noexcept
  {
    outlet_long(p, v);
  }
  void operator()(std::string_view v) const noexcept
  {
    outlet_anything(p, gensym(v.data()), 0, nullptr);
  }
  void operator()(const std::string& v) const noexcept
  {
    outlet_anything(p, gensym(v.data()), 0, nullptr);
  }

  template<typename T>
    requires std::is_aggregate_v<T>
  void operator()(const T& v) const noexcept
  {
    if constexpr(avnd::has_field_names<T>)
    {
      aggregate_to_dict dict;
      dict(v);
      dictionary_dump(dict.d, true, true);
      object_free(dict.d);
    }
    else
    {
      to_list l;
      l(v);

      if(l.atoms.size()> std::numeric_limits<short>::max())
        l.atoms.resize(std::numeric_limits<short>::max());

      outlet_list(p, nullptr, (short)l.atoms.size(), l.atoms.data());
    }
  }

  void operator()(const avnd::vector_ish auto& v) const noexcept
  {
    to_list l;
    l(v);
    outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
  }

  void operator()(const avnd::set_ish auto& v) const noexcept
  {
    to_list l;
    l(v);
    outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
  }

  void operator()(const avnd::variant_ish auto& v) const noexcept
  {
    using namespace std;
    visit([this](const auto& val) { (*this)(val); }, v);
  }

  void operator()(const avnd::map_ish auto& v) const noexcept
  {
    /*
    to_dict l;
    l(v);
    outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
    */
  }
  template <typename... Args>
    requires(sizeof...(Args) > 1)
  void operator()(Args&&... v) noexcept
  {
    std::array<t_atom, sizeof...(Args)> atoms;
    static constexpr int N = sizeof...(Args);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], v), ...);
    }(std::make_index_sequence<N>{});

    outlet_list(p, nullptr, N, atoms.data());
  }
};

// FIXME port the thread-local mechanism to pd in to_list
template <typename C, typename... Args>
inline void value_to_max_dispatch(t_outlet* outlet, Args&&... v) noexcept
{
  if constexpr(avnd::has_symbol<C> || avnd::has_c_name<C>)
  {
    static const auto sym = sget_static_symbol<C>();
    return do_value_to_max_anything{}(outlet, sym, std::forward<Args>(v)...);
  }
  else
  {
    return do_value_to_max_typed{}(outlet, std::forward<Args>(v)...);
  }
}

template <avnd::tag_dynamic_symbol C>
inline void value_to_max_dispatch(
    t_outlet* outlet, avnd::string_ish auto&& dsym, avnd::span_ish auto&& v) noexcept
{
  static thread_local std::vector<t_atom> atoms;
  const int N = v.size();
  atoms.clear();
  atoms.resize(N);

  for(int i = 0; i < N; i++)
  {
    value_to_max(atoms[i], v[i]);
  }
  outlet_anything(outlet, gensym(dsym.data()), N, atoms.data());
}

template <avnd::tag_dynamic_symbol C, typename... Args>
inline void value_to_max_dispatch(t_outlet* outlet, Args&&... v) noexcept
{
  [outlet]<typename... TArgs>(const avnd::string_ish auto& dsym, TArgs&&... args) {
    std::array<t_atom, sizeof...(TArgs)> atoms;
    static constexpr int N = sizeof...(TArgs);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (value_to_max(atoms[I], args), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(outlet, gensym(dsym.data()), N, atoms.data());
  }(v...);
}

template <typename C, typename... Args>
void dispatch_outlet(C& c, t_outlet* outlet, Args&&... args)
{
  do_process_outlet{p}(std::forward<Args>(args)...);
}

template <typename T>
struct outputs
{
  template <typename R, typename... Args, template <typename...> typename F>
  static void init_func_view(F<R(Args...)>& call, t_outlet& outlet)
  {
    call.context = &outlet;
    call.function = [](void* ptr, Args... args) {
      t_outlet* p = static_cast<t_outlet*>(ptr);
      do_process_outlet{p}(std::forward<Args>(args)...);
    };
  }

  template <typename R, typename... Args, template <typename...> typename F>
  static void init_func(F<R(Args...)>& call, t_outlet& outlet)
  {
    call = [&outlet](Args... args)  {
      do_process_outlet{&outlet}(std::forward<Args>(args)...);
    };
  }

  template <avnd::callback C>
  static void setup(C& out, t_outlet& outlet)
  {
    using call_type = decltype(C::call);
    if constexpr(avnd::function_view_ish<call_type>)
    {
      init_func_view(out.call, outlet);
    }
    else if constexpr(avnd::function_ish<call_type>)
    {
      init_func(out.call, outlet);
    }
  }

  template <typename Out>
  void setup(Out& out, t_outlet& outlet)
  {
  }

  void commit(avnd::effect_container<T>& implementation)
  {
    int k = 0;
    avnd::output_introspection<T>::for_all(
        avnd::get_outputs<T>(implementation), [this, &k]<typename C>(C& ctl) {
          if constexpr(requires(float v) { v = ctl.value; })
          {
            outlet_float(outlets[k], ctl.value);
          }
          ++k;
        });
  }

  void init(avnd::effect_container<T>& implementation, t_object& x_obj)
  {
    constexpr auto N = avnd::output_introspection<T>::size;
    if constexpr(N > 0)
    {
      int out_k = 0;
      std::array<t_symbol*, N> names;

      // outlet_new is right-to-left so we create in reverse order...
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs<T>(implementation), [&names, &out_k](auto& ctl) {
            names[out_k++] = symbol_for_port(ctl);
          });

      out_k = N;
      for(auto it = names.crbegin(); it != names.crend(); ++it) {
        const auto& name = *it;
        outlets[--out_k] = static_cast<t_outlet*>(outlet_new(&x_obj, name->s_name));
      }

      out_k = 0;
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs<T>(implementation), [this, &out_k](auto& ctl) {
            this->setup(ctl, *outlets[out_k]);
            out_k++;
          });
    }
  }

  std::array<t_outlet*, avnd::output_introspection<T>::size> outlets;
};

}
