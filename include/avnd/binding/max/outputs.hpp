#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/dict.hpp>
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
    outlet_int(p, v);
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
      to_list l;
      l(v);

      if(l.atoms.size()> std::numeric_limits<short>::max())
        l.atoms.resize(std::numeric_limits<short>::max());

      outlet_list(p, nullptr, (short)l.atoms.size(), l.atoms.data());

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

/// Versions for the paramter (one-value) case
// One-arg overload to handle the dict case
template <parameter_with_field_names C, typename T, std::size_t Idx, typename Arg>
inline void value_to_max_dispatch(T& self, avnd::field_index<Idx> idx, t_outlet* outlet, Arg&& v) noexcept
{
  // 1. Get the dict from object storage
  const dict_state& storage = self.dicts.get_output(idx);

  // 2. Update it
  aggregate_to_dict dict{storage.d};
  dict(v);

  // 3. Output it
  t_atom a;
  atom_setsym(&a, storage.s);
  outlet_anything(outlet, _sym_dictionary, 1, &a);
}

template <typename C, typename T, std::size_t Idx, typename Arg>
inline void value_to_max_dispatch(T& self, avnd::field_index<Idx>, t_outlet* outlet, Arg&& v) noexcept
{
  if constexpr(avnd::has_symbol<C> || avnd::has_c_name<C>)
  {
    // FIXME
    static const auto sym = get_static_symbol<C>();
    // return do_value_to_max_anything{}(outlet, sym, std::forward<Arg>(v));
  }
  //else
  {
    return do_value_to_max_typed{outlet}(std::forward<Arg>(v));
  }
}

template <avnd::tag_dynamic_symbol C, typename T, std::size_t Idx, typename Arg>
inline void value_to_max_dispatch(T& self, avnd::field_index<Idx>, t_outlet* outlet, avnd::string_ish auto&& dsym) noexcept
{
  outlet_anything(outlet, gensym(dsym.data()), 0, nullptr);
}

/// Versions only for the avnd::callback cases
template <typename C,typename... Args>
inline void value_to_max_dispatch(t_outlet* outlet, Args&&... v) noexcept
{
  if constexpr(avnd::has_symbol<C> || avnd::has_c_name<C>)
  {
    // FIXME
    static const auto sym = get_static_symbol<C>();
    // return do_value_to_max_anything{}(outlet, sym, std::forward<Args>(v)...);
  }
  //else
  {
    return do_value_to_max_typed{outlet}(std::forward<Args>(v)...);
  }
}

template <avnd::tag_dynamic_symbol C>
inline void value_to_max_dispatch(
                                  t_outlet* outlet, avnd::string_ish auto&& dsym, avnd::span_ish auto&& v) noexcept
{
  to_list conv;
  conv(v);
  outlet_anything(outlet, gensym(dsym.data()), conv.atoms.size(), conv.atoms.data());
}

template <avnd::tag_dynamic_symbol C,typename Arg>
inline void value_to_max_dispatch(t_outlet* outlet, avnd::string_ish auto&& dsym) noexcept
{
  outlet_anything(outlet, gensym(dsym.data()), 0, nullptr);
}

template <avnd::tag_dynamic_symbol C, typename... Args>
inline void value_to_max_dispatch(t_outlet* outlet, Args&&... v) noexcept
{
  [outlet]<typename... TArgs>(const avnd::string_ish auto& dsym, TArgs&&... args) {
    std::array<t_atom, sizeof...(TArgs)> atoms;
    static constexpr int N = sizeof...(TArgs);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], args), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(outlet, gensym(dsym.data()), N, atoms.data());
  }(v...);
}

template <typename T>
struct value_writer
{
  T& self;

  template <avnd::parameter Field, std::size_t Idx>
    requires(!avnd::sample_accurate_parameter<Field>)
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
  {
    value_to_max_dispatch<Field>(self, avnd::field_index<Idx>{}, port, ctrl.value);
  }

  template <avnd::linear_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
  {
    // FIXME
#if 0
    auto& buffers = self.control_buffers.linear_inputs;
    // Idx is the index of the port in the complete input array.
    // We need to map it to the linear input index.
    using processor_type = typename T::processor_type;
    using lin_out = avnd::linear_timed_parameter_output_introspection<processor_type>;
    using indices = typename lin_out::indices_n;
    static constexpr int storage_index = avnd::index_of_element<Idx>(indices{});

    auto& buffer = get<storage_index>(buffers);

    for(int i = 0, N = self.buffer_size; i < N; i++)
    {
      if(buffer[i])
      {
        value_to_max_dispatch<Field>(self, avnd::field_index<Idx>{}, port, *buffer[i]);
        buffer[i] = {};
      }
    }
#endif
  }

  template <avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
  {
    for(auto& [timestamp, val] : ctrl.values)
    {
      value_to_max_dispatch<Field>(self, avnd::field_index<Idx>{}, port, val);
    }
    ctrl.values.clear();
  }

  // does not make sense as output, only as input
  template <avnd::span_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept = delete;

  void operator()(auto&&...) const noexcept { }
};

template <typename T>
struct outputs
{
  template <typename C, typename R, typename... Args, template <typename...> typename F>
  static void init_func_view(F<R(Args...)>& call, t_outlet& outlet)
  {
    call.context = &outlet;
    call.function = [](void* ptr, Args... args) {
      t_outlet* p = static_cast<t_outlet*>(ptr);
      value_to_max_dispatch<C>(p, std::forward<Args>(args)...);
    };
  }

  template <typename C, typename R, typename... Args, template <typename...> typename F>
  static void init_func(F<R(Args...)>& call, t_outlet& outlet)
  {
    call = [&outlet](Args... args)  {
      value_to_max_dispatch<C>(&outlet, std::forward<Args>(args)...);
    };
  }

  template <avnd::callback C>
  static void setup(T& self, C& out, t_outlet& outlet)
  {
    using call_type = decltype(C::call);
    if constexpr(avnd::function_view_ish<call_type>)
    {
      init_func_view<C>(out.call, outlet);
    }
    else if constexpr(avnd::function_ish<call_type>)
    {
      init_func<C>(out.call, outlet);
    }
  }

  template <typename Out>
  void setup(Out& out, t_outlet& outlet)
  {
  }

  template <typename Self>
  void commit(Self& self)
  {
    using info = avnd::output_introspection<T>;
    if constexpr(info::size > 0)
    {
      // FIXME stops being correct if we loose the avnd port <-> outlet correspondance
      // with e.g. multiple callbacks in a single port
      auto& outs = avnd::get_outputs<T>(self.implementation);
      [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        (value_writer<Self>{self}(
             avnd::pfr::get<Index>(outs), outlets[Index], avnd::num<Index>{}),
         ...);
      }(typename info::indices_n{});
    }
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
          avnd::get_outputs<T>(implementation), [&names, &out_k]<typename C>(C& ctl) {
            names[out_k++] = symbol_for_port<C>();
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
