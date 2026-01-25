#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/pd/helpers.hpp>
#include <avnd/common/enum_reflection.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/vecf.hpp>
#include <boost/container/small_vector.hpp>

#include <array>
#include <vector>
namespace pd
{

template <typename T>
  requires std::is_arithmetic_v<T>
inline void value_to_pd(t_atom& atom, T v) noexcept
{
  atom = {.a_type = A_FLOAT, .a_w = {.w_float = (t_float)v}};
}
inline void value_to_pd(t_atom& atom, bool v) noexcept
{
  atom = {.a_type = A_FLOAT, .a_w = {.w_float = v ? 1.0f : 0.0f}};
}
template <typename T>
  requires std::is_enum_v<T>
inline void value_to_pd(t_atom& atom, T v) noexcept
{
  atom = {
      .a_type = A_SYMBOL, .a_w = {.w_symbol = gensym(magic_enum::enum_name(v).data())}};
}
inline void value_to_pd(t_atom& atom, const char* v) noexcept
{
  atom = {.a_type = A_SYMBOL, .a_w = {.w_symbol = gensym(v)}};
}
inline void value_to_pd(t_atom& atom, std::string_view v) noexcept
{
  atom = {.a_type = A_SYMBOL, .a_w = {.w_symbol = gensym(v.data())}};
}
inline void value_to_pd(t_atom& atom, const std::string& v) noexcept
{
  atom = {.a_type = A_SYMBOL, .a_w = {.w_symbol = gensym(v.c_str())}};
}
template<typename T>
inline void value_to_pd(t_atom& atom, const std::optional<T>& v) noexcept
{
  if(v)
    value_to_pd(atom, *v);
  else
    atom = {.a_type = A_NULL, .a_w = {}};
}

struct do_value_to_pd_rec
{
  // FIXME static thread_local errors with msvc...
  boost::container::small_vector<t_atom, 256> atoms;

  template <typename T>
    requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T>)
  void operator()(const T& v)
  {
    static constexpr int sz = avnd::pfr::tuple_size_v<T>;

    if constexpr(sz == 0)
    {
      atoms.push_back({.a_type = A_NULL, .a_w = {.w_float = 0}});
    }
    else
    {
      avnd::for_each_field_ref_n(
          v, [this]<std::size_t I>(auto& field, avnd::field_index<I>) mutable {
        (*this)(field);
      });
    }
  }

  void operator()() noexcept
  {
    atoms.push_back({.a_type = A_NULL, .a_w = {.w_float = 0}});
  }
  void operator()(std::floating_point auto v) noexcept
  {
    atoms.push_back({.a_type = A_FLOAT, .a_w = {.w_float = (t_float)v}});
  }
  void operator()(std::integral auto v) noexcept
  {
    atoms.push_back({.a_type = A_FLOAT, .a_w = {.w_float = (t_float)v}});
  }
  template <typename T>
    requires std::is_enum_v<T>
  void operator()(T v) noexcept
  {
    atoms.push_back(
        {.a_type = A_SYMBOL,
         .a_w = {.w_symbol = gensym(magic_enum::enum_name(v).data())}});
  }
  void operator()(std::string_view v) noexcept
  {
    atoms.push_back({.a_type = A_SYMBOL, .a_w = {.w_symbol = gensym(v.data())}});
  }
  void operator()(const std::string& v) noexcept
  {
    atoms.push_back({.a_type = A_SYMBOL, .a_w = {.w_symbol = gensym(v.data())}});
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
    using namespace std;
    if(v)
      (*this)(*v);
    else
      atoms.push_back({.a_type = A_NULL, .a_w = {.w_float = 0}});
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

struct do_value_to_pd_typed
{
  void operator()(t_outlet* outlet) const noexcept { outlet_bang(outlet); }

  template <typename T>
    requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T>)
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    static constexpr int sz = avnd::pfr::tuple_size_v<T>;

    if constexpr(sz == 0)
    {
      outlet_bang(outlet);
    }
    else
    {
      static constexpr auto N = avnd::pfr::tuple_size_v<T>;
      std::array<t_atom, N> atoms;
      avnd::for_each_field_ref_n(
          v, [&atoms]<std::size_t I>(auto& field, avnd::field_index<I>) mutable {
        value_to_pd(atoms[I], field);
      });
      outlet_list(outlet, &s_list, N, atoms.data());
    }
  }

  template <avnd::tuple_ish T>
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    static constexpr int N = std::tuple_size_v<T>;
    std::array<t_atom, N> atoms;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (value_to_pd(atoms[I], std::get<I>(v)), ...);
    }(std::make_index_sequence<N>{});

    outlet_list(outlet, &s_list, N, atoms.data());
  }

  template <typename T>
    requires avnd::span_ish<T>
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    static_assert(convertible_to_fundamental_value_type<typename T::value_type>);
    // FIXME does not work for recursivity: static thread_local. We need a pool.
    static thread_local std::vector<t_atom> atoms;
    const int N = v.size();
    atoms.clear();
    atoms.resize(N);

    for(int i = 0; i < N; i++)
    {
      value_to_pd(atoms[i], v[i]);
    }

    outlet_list(outlet, &s_list, N, atoms.data());
  }

  template <typename T, std::size_t N>
  void operator()(t_outlet* outlet, const std::array<T, N>& v) const noexcept
  {
    static_assert(convertible_to_fundamental_value_type<T>);
    std::array<t_atom, N> atoms;

    for(int i = 0; i < N; i++)
    {
      value_to_pd(atoms[i], v[i]);
    }

    outlet_list(outlet, &s_list, v.size(), atoms.data());
  }

  template <std::size_t N>
  void operator()(t_outlet* outlet, const avnd::array_ish<N> auto& v) const noexcept
  {
    std::array<t_atom, N> atoms;

    for(int i = 0; i < N; i++)
    {
      value_to_pd(atoms[i], v[i]);
    }

    outlet_list(outlet, &s_list, N, atoms.data());
  }

  template <typename T>
    requires((avnd::set_ish<T> || avnd::list_ish<T>) && !avnd::vector_ish<T>)
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    static thread_local std::vector<t_atom> atoms;
    const int N = v.size();
    atoms.clear();
    atoms.resize(N);

    int i = 0;
    for(auto& element : v)
    {
      value_to_pd(atoms[i++], element);
    }
    outlet_list(outlet, &s_list, v.size(), atoms.data());
  }

  template <typename T>
    requires avnd::map_ish<T>
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    static thread_local std::vector<t_atom> atoms;
    const int N = v.size();
    atoms.clear();
    atoms.resize(2 * N);

    int i = 0;
    for(auto& [key, value] : v)
    {
      value_to_pd(atoms[i++], key);
      value_to_pd(atoms[i++], value);
    }
    outlet_list(outlet, &s_list, v.size(), atoms.data());
  }

  template <typename T>
    requires avnd::variant_ish<T>
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    using namespace std;
    visit([&](const auto& element) { (*this)(outlet, element); }, v);
  }

  template <typename T>
    requires avnd::pair_ish<T>
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    t_atom atoms[2];

    value_to_pd(atoms[0], v.first);
    value_to_pd(atoms[1], v.second);

    outlet_list(outlet, &s_list, 2, atoms);
  }

  template <typename T>
    requires avnd::optional_ish<T>
  void operator()(t_outlet* outlet, const T& v) const noexcept
  {
    if(v)
      (*this)(outlet, *v);
  }

  inline void operator()(t_outlet* outlet, std::integral auto v) const noexcept
  {
    outlet_float(outlet, v);
  }
  inline void operator()(t_outlet* outlet, std::floating_point auto v) const noexcept
  {
    outlet_float(outlet, v);
  }
  inline void operator()(t_outlet* outlet, bool v) const noexcept
  {
    outlet_float(outlet, v ? 1.f : 0.f);
  }
  inline void operator()(t_outlet* outlet, const char* v) const noexcept
  {
    outlet_symbol(outlet, gensym(v));
  }
  inline void operator()(t_outlet* outlet, std::string_view v) const noexcept
  {
    outlet_symbol(outlet, gensym(v.data()));
  }
  inline void operator()(t_outlet* outlet, const std::string& v) const noexcept
  {
    outlet_symbol(outlet, gensym(v.c_str()));
  }
  void operator()(t_outlet* outlet, const avnd::bitset_ish auto& v) const noexcept
  {
    boost::container::small_vector<t_atom, 512> atoms;
    const int N = v.size();
    atoms.resize(2 * N);

    for(int i = 0, N = v.size(); i < N; i++)
    {
      value_to_pd(atoms[i], v.test(i) ? 1 : 0);
    }

    outlet_list(outlet, &s_list, N, atoms.data());
  }

  template <typename... Args>
    requires(sizeof...(Args) > 1)
  inline void operator()(t_outlet* outlet, Args&&... v) const noexcept
  {
    std::array<t_atom, sizeof...(Args)> atoms;
    static constexpr int N = sizeof...(Args);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (value_to_pd(atoms[I], v), ...);
    }(std::make_index_sequence<N>{});

    outlet_list(outlet, &s_list, N, atoms.data());
  }
};

struct do_value_to_pd_anything
{
  void operator()(t_outlet* outlet, t_symbol* sym) const noexcept
  {
    outlet_anything(outlet, sym, 0, nullptr);
  }

  template <typename T>
    requires(std::is_aggregate_v<T> && !avnd::iterable_ish<T> && !avnd::tuple_ish<T>)
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    static constexpr int sz = avnd::pfr::tuple_size_v<T>;

    if constexpr(sz == 0)
    {
      outlet_anything(outlet, sym, 0, nullptr);
    }
    else
    {
      // FIXME for pd we need to flatten outputs as there
      // is no data-structure nesting
      static constexpr auto N = avnd::pfr::tuple_size_v<T>;
      std::array<t_atom, N> atoms;
      avnd::for_each_field_ref_n(
          v, [&atoms]<std::size_t I>(auto& field, avnd::field_index<I>) mutable {
        value_to_pd(atoms[I], field);
      });
      outlet_anything(outlet, sym, N, atoms.data());
    }
  }

  template <typename T>
    requires avnd::span_ish<T>
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    static thread_local std::vector<t_atom> atoms;
    const int N = v.size();
    atoms.clear();
    atoms.resize(N);

    for(int i = 0; i < N; i++)
    {
      value_to_pd(atoms[i], v[i]);
    }

    outlet_anything(outlet, sym, v.size(), atoms.data());
  }

  // FIXME msvc isn't able to match the more generic template
  template <typename T, std::size_t N>
  void
  operator()(t_outlet* outlet, t_symbol* sym, const std::array<T, N>& v) const noexcept
  {
    std::array<t_atom, N> atoms;

    for(int i = 0; i < N; i++)
    {
      value_to_pd(atoms[i], v[i]);
    }

    outlet_anything(outlet, sym, v.size(), atoms.data());
  }

  template <std::size_t N>
  void operator()(
      t_outlet* outlet, t_symbol* sym, const avnd::array_ish<N> auto& v) const noexcept
  {
    std::array<t_atom, N> atoms;

    for(int i = 0; i < N; i++)
    {
      value_to_pd(atoms[i], v[i]);
    }

    outlet_anything(outlet, sym, v.size(), atoms.data());
  }

  template <typename T>
    requires((avnd::set_ish<T> || avnd::list_ish<T>) && !avnd::vector_ish<T>)
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    // FIXME that does not work, we can have recursive types
    static thread_local std::vector<t_atom> atoms;
    const int N = v.size();
    atoms.clear();
    atoms.resize(N);

    int i = 0;
    for(auto& element : v)
    {
      value_to_pd(atoms[i++], element);
    }
    outlet_anything(outlet, sym, v.size(), atoms.data());
  }

  template <typename T>
    requires avnd::map_ish<T>
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    static thread_local std::vector<t_atom> atoms;
    const int N = v.size();
    atoms.clear();
    atoms.resize(2 * N);

    int i = 0;
    for(auto& [key, value] : v)
    {
      value_to_pd(atoms[i++], key);
      value_to_pd(atoms[i++], value);
    }
    outlet_anything(outlet, sym, v.size(), atoms.data());
  }

  template <typename T>
    requires avnd::variant_ish<T>
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    using namespace std;
    visit([&](const auto& element) { (*this)(outlet, sym, element); }, v);
  }

  template <typename T>
    requires avnd::pair_ish<T>
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    t_atom atoms[2];

    value_to_pd(atoms[0], v.first);
    value_to_pd(atoms[1], v.second);

    outlet_anything(outlet, sym, 2, atoms);
  }

  template <typename T>
    requires avnd::optional_ish<T>
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    if(v)
      (*this)(outlet, sym, *v);
  }

  inline void
  operator()(t_outlet* outlet, t_symbol* sym, std::integral auto v) const noexcept
  {
    t_atom atom;
    value_to_pd(atom, v);
    outlet_anything(outlet, sym, 1, &atom);
  }

  inline void
  operator()(t_outlet* outlet, t_symbol* sym, std::floating_point auto v) const noexcept
  {
    t_atom atom;
    value_to_pd(atom, v);
    outlet_anything(outlet, sym, 1, &atom);
  }

  inline void operator()(t_outlet* outlet, t_symbol* sym, bool v) const noexcept
  {
    t_atom atom;
    value_to_pd(atom, v);
    outlet_anything(outlet, sym, 1, &atom);
  }

  inline void operator()(t_outlet* outlet, t_symbol* sym, const char* v) const noexcept
  {
    t_atom atom;
    value_to_pd(atom, v);
    outlet_anything(outlet, sym, 1, &atom);
  }

  inline void
  operator()(t_outlet* outlet, t_symbol* sym, std::string_view v) const noexcept
  {
    t_atom atom;
    value_to_pd(atom, v);
    outlet_anything(outlet, sym, 1, &atom);
  }

  inline void
  operator()(t_outlet* outlet, t_symbol* sym, const std::string& v) const noexcept
  {
    t_atom atom;
    value_to_pd(atom, v);
    outlet_anything(outlet, sym, 1, &atom);
  }

  void operator()(
      t_outlet* outlet, t_symbol* sym, const avnd::bitset_ish auto& v) const noexcept
  {
    boost::container::small_vector<t_atom, 512> atoms;
    const int N = v.size();
    atoms.resize(2 * N);

    for(int i = 0, N = v.size(); i < N; i++)
    {
      value_to_pd(atoms[i], v.test(i) ? 1 : 0);
    }

    outlet_anything(outlet, sym, N, atoms.data());
  }

  template <avnd::tuple_ish T>
  void operator()(t_outlet* outlet, t_symbol* sym, const T& v) const noexcept
  {
    static constexpr int N = std::tuple_size_v<T>;
    std::array<t_atom, N> atoms;

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (value_to_pd(atoms[I], std::get<I>(v)), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(outlet, sym, N, atoms.data());
  }

  template <typename... Args>
    requires(sizeof...(Args) > 1)
  inline void operator()(t_outlet* outlet, t_symbol* sym, Args&&... v) const noexcept
  {
    std::array<t_atom, sizeof...(Args)> atoms;
    static constexpr int N = sizeof...(Args);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (value_to_pd(atoms[I], v), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(outlet, sym, N, atoms.data());
  }
};

template <typename C, typename... Args>
inline void value_to_pd_dispatch(t_outlet* outlet, Args&&... v) noexcept
{
  if constexpr(avnd::has_symbol<C> || avnd::has_c_name<C>)
  {
    static const auto sym = get_message_out_symbol<C>();
    if constexpr((convertible_to_atom_list_statically<Args> && ...))
    {
      return do_value_to_pd_anything{}(outlet, sym, std::forward<Args>(v)...);
    }
    else
    {
      do_value_to_pd_rec rec{};
      (rec(v), ...);
      outlet_anything(outlet, sym, rec.atoms.size(), rec.atoms.data());
      return;
    }
  }
  else
  {
    if constexpr((convertible_to_atom_list_statically<Args> && ...))
    {
      return do_value_to_pd_typed{}(outlet, std::forward<Args>(v)...);
    }
    else
    {
      do_value_to_pd_rec rec{};
      (rec(v), ...);
      outlet_list(outlet, &s_list, rec.atoms.size(), rec.atoms.data());
      return;
    }
  }
}

template <avnd::tag_dynamic_symbol C>
inline void value_to_pd_dispatch(
    t_outlet* outlet, avnd::string_ish auto&& dsym, avnd::span_ish auto&& v) noexcept
{
  static thread_local std::vector<t_atom> atoms;
  const int N = v.size();
  atoms.clear();
  atoms.resize(N);

  for(int i = 0; i < N; i++)
  {
    value_to_pd(atoms[i], v[i]);
  }
  outlet_anything(outlet, gensym(dsym.data()), N, atoms.data());
}

template <avnd::tag_dynamic_symbol C, typename... Args>
inline void value_to_pd_dispatch(t_outlet* outlet, Args&&... v) noexcept
{
  [outlet]<typename... TArgs>(const avnd::string_ish auto& dsym, TArgs&&... args) {
    std::array<t_atom, sizeof...(TArgs)> atoms;
    static constexpr int N = sizeof...(TArgs);

    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (value_to_pd(atoms[I], args), ...);
    }(std::make_index_sequence<N>{});

    outlet_anything(outlet, gensym(dsym.data()), N, atoms.data());
  }(v...);
}

template <typename T>
struct value_writer
{
  T& self;

  template <avnd::parameter_port Field, std::size_t Idx>
    requires(!avnd::sample_accurate_parameter<Field>)
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
  {
    value_to_pd_dispatch<Field>(port, ctrl.value);
  }

  template <avnd::linear_sample_accurate_parameter_port Field, std::size_t Idx>
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
  {
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
        value_to_pd_dispatch<Field>(port, *buffer[i]);
        buffer[i] = {};
      }
    }
  }

  template <avnd::dynamic_sample_accurate_parameter_port Field, std::size_t Idx>
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
  {
    for(auto& [timestamp, val] : ctrl.values)
    {
      value_to_pd_dispatch<Field>(port, val);
    }
    ctrl.values.clear();
  }

  // does not make sense as output, only as input
  template <avnd::span_sample_accurate_parameter_port Field, std::size_t Idx>
  void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept = delete;

  void operator()(auto&&...) const noexcept { }
};

template <typename T>
struct outputs
{
  template <typename C, typename R, typename... Args, template <typename...> typename F>
  static void init_func_view(const C&, F<R(Args...)>& call, t_outlet& outlet)
  {
    call.context = &outlet;
    call.function = [](void* ptr, Args... args) {
      t_outlet* p = static_cast<t_outlet*>(ptr);
      value_to_pd_dispatch<C>(p, std::forward<Args>(args)...);
    };
  }

  template <typename C, typename R, typename... Args, template <typename...> typename F>
  static void init_func(const C&, F<R(Args...)>& call, t_outlet& outlet)
  {
    call = [&outlet](Args... args) {
      value_to_pd_dispatch<C>(&outlet, std::forward<Args>(args)...);
    };
  }

  template <avnd::callback C>
  static void setup(C& out, t_outlet& outlet)
  {
    using call_type = decltype(C::call);
    if constexpr(avnd::function_view_ish<call_type>)
    {
      init_func_view(out, out.call, outlet);
    }
    else if constexpr(avnd::function_ish<call_type>)
    {
      init_func(out, out.call, outlet);
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
    using info = avnd::output_introspection<T>;
    if constexpr(info::size > 0)
    {
      int out_k = 0;
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs<T>(implementation),
          [this, &out_k, &x_obj]<typename Field>(Field& ctl) {
        outlets[out_k] = outlet_new(&x_obj, symbol_for_port<Field>());
        this->setup(ctl, *outlets[out_k]);
        out_k++;
      });
    }
  }

  std::array<t_outlet*, avnd::output_introspection<T>::size> outlets;
};
}
