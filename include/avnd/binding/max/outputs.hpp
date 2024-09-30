#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/dict.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/to_atoms.hpp>
#include <boost/container/small_vector.hpp>
#include <commonsyms.h>
namespace max
{

template <std::integral T>
inline void value_to_max(t_atom& atom, T v) noexcept
{
  atom = {.a_type = A_LONG, .a_w = {.w_long = v}};
}
template <std::floating_point T>
inline void value_to_max(t_atom& atom, T v) noexcept
{
  atom = {.a_type = A_FLOAT, .a_w = {.w_float = v}};
}
inline void value_to_max(t_atom& atom, bool v) noexcept
{
  atom = {.a_type = A_LONG, .a_w = {.w_long= v ? 1 : 0}};
}
inline void value_to_max(t_atom& atom, const char* v) noexcept
{
  atom = {.a_type = A_SYM, .a_w = {.w_sym = gensym(v)}};
}
inline void value_to_max(t_atom& atom, std::string_view v) noexcept
{
  atom = {.a_type = A_SYM, .a_w = {.w_sym = gensym(v.data())}};
}
inline void value_to_max(t_atom& atom, const std::string& v) noexcept
{
  atom = {.a_type = A_SYM, .a_w = {.w_sym = gensym(v.c_str())}};
}
template <typename T>
  requires std::is_enum_v<T>
inline void value_to_max(t_atom& atom, T v) noexcept
{
  atom = {.a_type = A_SYM, .a_w = {.w_sym = gensym(magic_enum::enum_name(v).data())}};
}

struct do_value_to_max_rec
{
  // FIXME static thread_local errors with msvc...
  boost::container::small_vector<t_atom, 256> atoms;

  template <typename T>
    requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T>)
  void operator()(const T& v)
  {
    avnd::for_each_field_ref_n(
        v, [this]<std::size_t I>(auto& field, avnd::field_index<I>) mutable {
      (*this)(field);
    });
  }

  void operator()() noexcept
  {
    atoms.push_back({.a_type = A_NOTHING, .a_w = {.w_long = 0}});
  }
  void operator()(std::floating_point auto v) noexcept
  {
    atoms.push_back({.a_type = A_FLOAT, .a_w = {.w_float = v}});
  }
  void operator()(std::integral auto v) noexcept
  {
    atoms.push_back({.a_type = A_LONG, .a_w = {.w_long = v}});
  }
  void operator()(std::string_view v) noexcept
  {
    atoms.push_back({.a_type = A_SYM, .a_w = {.w_sym = gensym(v.data())}});
  }
  void operator()(const std::string& v) noexcept
  {
    atoms.push_back({.a_type = A_SYM, .a_w = {.w_sym = gensym(v.data())}});
  }
  template <typename T>
    requires std::is_enum_v<T>
  void operator()(T v) noexcept
  {
    atoms.push_back(
        {.a_type = A_SYM, .a_w = {.w_sym = gensym(magic_enum::enum_name(v).data())}});
  }

  void operator()(const avnd::iterable_ish auto& v) noexcept
  {
    atoms.reserve(atoms.size() + v.size());
    for(auto& e : v)
    {
      (*this)(e);
    }
  }

  void operator()(const avnd::map_ish auto& v) noexcept
  {
    atoms.reserve(atoms.size() + v.size() * 2);
    for(auto& [k, v] : v)
    {
      (*this)(k);
      (*this)(v);
    }
  }

  void operator()(const avnd::variant_ish auto& v) noexcept
  {
    using namespace std;
    visit([this](const auto& val) { (*this)(val); }, v);
  }

  void operator()(const avnd::optional_ish auto& v) noexcept
  {
    if(v)
      (*this)(*v);
  }

  void operator()(const avnd::pair_ish auto& v) noexcept
  {
    (*this)(v.first);
    (*this)(v.second);
  }

  template <avnd::tuple_ish T>
    requires(!avnd::iterable_ish<T>)
  void operator()(const T& v) noexcept
  {
    static constexpr int N = std::tuple_size_v<T>;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((*this)(std::get<I>(v)), ...);
    }(std::make_index_sequence<N>{});
  }
};

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
  template <typename T>
    requires std::is_enum_v<T>
  void operator()(T v) const noexcept
  {
    t_atom atom;
    value_to_max(atom, v);
    outlet_anything(p, _sym_symbol, 1, &atom);
  }
  void operator()(std::string_view v) const noexcept
  {
    outlet_anything(p, gensym(v.data()), 0, nullptr);
  }
  void operator()(const std::string& v) const noexcept
  {
    outlet_anything(p, gensym(v.data()), 0, nullptr);
  }

  template <typename T>
    requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T>)
  void operator()(const T& v) const noexcept
  {
    to_list l;
    l(v);

    if(l.atoms.size()> std::numeric_limits<short>::max())
      l.atoms.resize(std::numeric_limits<short>::max());

    outlet_list(p, nullptr, (short)l.atoms.size(), l.atoms.data());
  }

  template <std::size_t N>
  void operator()(const avnd::array_ish<N> auto& v) const noexcept
  {
    std::array<t_atom, N> atoms;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], v[I]), ...);
    }(std::make_index_sequence<N>{});

    outlet_list(p, nullptr, N, atoms.data());
  }

  void operator()(const avnd::iterable_ish auto& v) const noexcept
  {
    to_list l;
    l(v);
    outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
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

  void operator()(const avnd::optional_ish auto& v) const noexcept
  {
    if(v)
      (*this)(*v);
  }

  void operator()(const avnd::variant_ish auto& v) const noexcept
  {
    using namespace std;
    visit([this](const auto& val) { (*this)(val); }, v);
  }

  void operator()(const avnd::map_ish auto& v) const noexcept
  {
    to_list l;
    l(v);
    outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
  }

  template <typename T>
    requires avnd::pair_ish<T>
  void operator()(const T& v) const noexcept
  {
    t_atom atoms[2];

    value_to_max(atoms[0], v.first);
    value_to_max(atoms[1], v.second);

    outlet_list(p, nullptr, 2, atoms);
  }

  template <avnd::tuple_ish T>
    requires(!avnd::iterable_ish<T> && !avnd::pair_ish<T>)
  void operator()(const T& v) const noexcept
  {
    static constexpr int N = std::tuple_size_v<T>;
    std::array<t_atom, N> atoms;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], std::get<I>(v)), ...);
    }(std::make_index_sequence<N>{});

    outlet_list(p, nullptr, N, atoms.data());
  }

  void operator()(const avnd::bitset_ish auto& v) const noexcept
  {
    boost::container::small_vector<t_atom, 512> atoms;
    const int N = v.size();
    atoms.resize(2 * N);

    for(int i = 0, N = v.size(); i < N; i++)
    {
      value_to_max(atoms[i], v.test(i) ? 1 : 0);
    }

    outlet_list(p, nullptr, atoms.size(), atoms.data());
  }

  template <typename... Args>
    requires(sizeof...(Args) > 1)
  void operator()(Args&&... v) const noexcept
  {
    std::array<t_atom, sizeof...(Args)> atoms;
    static constexpr int N = sizeof...(Args);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], v), ...);
    }(std::make_index_sequence<N>{});

    outlet_list(p, nullptr, N, atoms.data());
  }
};

struct do_value_to_max_anything
{
  t_outlet* p;
  t_symbol* s;

  void operator()() const noexcept
  {
    outlet_anything(p, s, 0, nullptr);
  }
  void operator()(std::floating_point auto v) const noexcept
  {
    t_atom atom;
    value_to_max(atom, v);
    outlet_anything(p, s, 1, &atom);
  }
  void operator()(std::integral auto v) const noexcept
  {
    t_atom atom;
    value_to_max(atom, v);
    outlet_anything(p, s, 1, &atom);
  }
  void operator()(std::string_view v) const noexcept
  {
    t_atom atom;
    value_to_max(atom, v);
    outlet_anything(p, s, 1, &atom);
  }
  void operator()(const std::string& v) const noexcept
  {
    t_atom atom;
    value_to_max(atom, v);
    outlet_anything(p, s, 1, &atom);
  }
  template <typename T>
    requires std::is_enum_v<T>
  void operator()(T v) const noexcept
  {
    t_atom atom;
    value_to_max(atom, v);
    outlet_anything(p, s, 1, &atom);
  }

  template <typename T>
    requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T>)
  void operator()(const T& v) const noexcept
  {
    to_list l;
    l(v);

    if(l.atoms.size()> std::numeric_limits<short>::max())
      l.atoms.resize(std::numeric_limits<short>::max());

    outlet_anything(p, s, (short)l.atoms.size(), l.atoms.data());
  }

  template <std::size_t N>
  void operator()(const avnd::array_ish<N> auto& v) const noexcept
  {
    std::array<t_atom, N> atoms;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], v[I]), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(p, s, N, atoms.data());
  }

  template <avnd::tuple_ish T>
    requires(!avnd::iterable_ish<T> && !avnd::pair_ish<T>)
  void operator()(const T& v) const noexcept
  {
    static constexpr auto N = std::tuple_size_v<T>;
    std::array<t_atom, N> atoms;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], std::get<I>(v)), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(p, s, N, atoms.data());
  }

  void operator()(const avnd::iterable_ish auto& v) const noexcept
  {
    to_list l;
    l(v);
    outlet_anything(p, s, l.atoms.size(), l.atoms.data());
  }

  void operator()(const avnd::variant_ish auto& v) const noexcept
  {
    using namespace std;
    visit([this](const auto& val) { (*this)(val); }, v);
  }

  void operator()(const avnd::optional_ish auto& v) const noexcept
  {
    if(v)
      (*this)(*v);
  }

  template <typename T>
    requires avnd::pair_ish<T>
  void operator()(const T& v) const noexcept
  {
    t_atom atoms[2];

    value_to_max(atoms[0], v.first);
    value_to_max(atoms[1], v.second);

    outlet_anything(p, s, 2, atoms);
  }

  void operator()(const avnd::bitset_ish auto& v) const noexcept
  {
    boost::container::small_vector<t_atom, 512> atoms;
    const int N = v.size();
    atoms.resize(2 * N);

    for(int i = 0, N = v.size(); i < N; i++)
    {
      value_to_max(atoms[i], v.test(i) ? 1 : 0);
    }

    outlet_anything(p, s, atoms.size(), atoms.data());
  }

  template <typename... Args>
  void operator()(t_outlet*, Args&&... v) const noexcept = delete;

  template <typename... Args>
    requires(sizeof...(Args) > 1)
  void operator()(Args&&... v) const noexcept
  {
    std::array<t_atom, sizeof...(Args)> atoms;
    static constexpr int N = sizeof...(Args);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (set_atom{}(&atoms[I], v[I]), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(p, s, N, atoms.data());
  }
};
// FIXME port the thread-local mechanism to pd in to_list

/// Versions for the paramter (one-value) case
// One-arg overload to handle the dict case
template <dict_parameter C, typename T, std::size_t Idx, typename Arg>
inline void value_to_max_dispatch(T& self, avnd::field_index<Idx> idx, t_outlet* outlet, Arg&& v) noexcept
{
  // 1. Get the dict from object storage
  const dict_state& storage = self.dicts.get_output(idx);

  // 2. Update it
  to_dict dict{storage.d};
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
    static const auto sym = get_message_out_symbol<C>();
    if constexpr(convertible_to_atom_list_statically<Arg>)
    {
      return do_value_to_max_anything{outlet, sym}(std::forward<Arg>(v));
    }
    else
    {
      do_value_to_max_rec rec{};
      rec(v);
      outlet_anything(outlet, sym, rec.atoms.size(), rec.atoms.data());
      return;
    }
  }
  else
  {
    if constexpr(convertible_to_atom_list_statically<Arg>)
    {
      return do_value_to_max_typed{outlet}(std::forward<Arg>(v));
    }
    else
    {
      do_value_to_max_rec rec{};
      rec(v);
      outlet_list(outlet, _sym_list, rec.atoms.size(), rec.atoms.data());
      return;
    }
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
    static const auto sym = get_message_out_symbol<C>();
    if constexpr((convertible_to_atom_list_statically<Args> && ...))
    {
      return do_value_to_max_anything{outlet, sym}(std::forward<Args>(v)...);
    }
    else
    {
      do_value_to_max_rec rec{};
      (rec(v), ...);
      outlet_anything(outlet, sym, rec.atoms.size(), rec.atoms.data());
      return;
    }
  }
  else
  {
    if constexpr((convertible_to_atom_list_statically<Args> && ...))
    {
      return do_value_to_max_typed{outlet}(std::forward<Args>(v)...);
    }
    else
    {
      do_value_to_max_rec rec{};
      (rec(v), ...);
      outlet_list(outlet, _sym_list, rec.atoms.size(), rec.atoms.data());
      return;
    }
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
  static void setup(C& out, t_outlet& outlet)
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
    else
    {
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
