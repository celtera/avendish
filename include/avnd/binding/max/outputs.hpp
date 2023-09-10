#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <avnd/binding/max/helpers.hpp>
#include <avnd/concepts/field_names.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/common/for_nth.hpp>

namespace max
{

template<std::size_t N>
auto to_symbols(const std::array<std::string_view, N>& array) noexcept
{
  std::array<t_symbol*, N> res;
  for(auto& [in, out] : std::ranges::zip_view(array, res))
  {
    out = gensym(in.data());
  }
  return res;
}

struct to_list
{
  t_outlet* outlet{};
  std::vector<t_atom> atoms;

  void add(int k, std::floating_point auto arg)  noexcept
  {
    atom_setfloat(atoms.data() + k, arg);
  }
  void add(int k,  std::integral auto arg)  noexcept
  {
    atom_setlong(atoms.data() + k, arg);
  }
  void add(int k, std::string_view v)  noexcept
  {
    atom_setsym(atoms.data() + k, gensym(v.data()));
  }
  void add(int k, const avnd::variant_ish auto& f)  noexcept
  {
    // TODO
  }
  void add(int k, const avnd::optional_ish auto& f)  noexcept
  {
    // TODO
  }

  template<typename F>
    requires std::is_aggregate_v<F>
  void operator()(const F& f)  noexcept
  {
    atoms.resize(boost::pfr::tuple_size_v<F>);

    avnd::for_each_field_ref(
        f, [this, k = 0](const auto& v) mutable{
          add(k++, v);
        });
  }

  void operator()(const avnd::vector_ish auto& f)  noexcept
  {
    int k = 0;
    atoms.resize(f.size());
    for(auto& v : f) {
      add(k++, v);
    }
  }

  void operator()(const avnd::set_ish auto& f)  noexcept
  {
    int k = 0;
    atoms.resize(f.size());
    for(auto& v : f) {
      add(k++, v);
    }
  }

  void operator()(const avnd::span_ish auto& f)  noexcept
  {
    int k = 0;
    atoms.resize(f.size());
    for(auto& v : f) {
      add(k++, v);
    }
  }

  void operator()(const avnd::map_ish auto& f)  noexcept
  {
    int k = 0;
    atoms.resize(2 * f.size());
    {
      for(auto& [key, v] : f) {
        add(k++, key);
        add(k++, v);
      }
    }
  }

  void operator()(const avnd::pair_ish auto& f)  noexcept
  {
    atoms.resize(2);
    add(0, f.first);
    add(1, f.second);
  }

  template<avnd::tuple_ish U>
  void operator()(const U& f)  noexcept
  {
    atoms.resize(std::tuple_size_v<U>);
    std::apply(
        [&, k = 0](auto&&... args) mutable { (add(k++, args), ...); },
        f);
  }

  void operator()(const avnd::bitset_ish auto& f)  noexcept
  {
    atoms.resize(f.size());
    for(int k = 0; k < f.size(); k++)
      atom_setlong(atoms.data() + k, f.test(k) ? 1 : 0);
  }

  void operator()(const auto& f) = delete;
};

struct aggregate_to_dict
{
  t_dictionary* d{};

  explicit aggregate_to_dict() noexcept
      : d{dictionary_new()}
  {

  }


  void add(t_symbol* k, std::floating_point auto v) noexcept
  {
    dictionary_appendfloat(d, k, v);
  }

  void add(t_symbol* k, std::integral auto v) noexcept
  {
    dictionary_appendlong(d, k, v);
  }

  void add(t_symbol* k, std::string_view v) noexcept
  {
    dictionary_appendstring(d, k, v.data());
  }

  template<typename T, std::size_t N>
  void add(t_symbol* k, std::array<T, N> v) noexcept
  {
  }

  template<typename F>
    requires avnd::has_field_names<F>
  void operator()(F&& f)
  {
    constexpr auto field_names = F::field_names();
    int k = 0;
    avnd::for_each_field_ref(
        f, [&](const auto& f) {
          constexpr auto name = field_names[k++];
          static const auto sym = gensym(name.data());
          add(sym, f);
        });
  }
};

struct do_process_outlet
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

  template<typename T>
    requires std::is_aggregate_v<T>
  void operator()(const T& v) const noexcept
  {
    if constexpr(avnd::has_field_names<T>)
    {
     aggregate_to_dict dict;
     dict(v);
    }
    else
    {
     to_list l;
     l(v);
     outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
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

  void operator()(const avnd::map_ish auto& v) const noexcept
  {
    /*
    to_dict l;
    l(v);
    outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
    */
  }

  void operator()(const auto& v) const noexcept = delete;
};


template <typename T>
struct outputs
{
  template <typename R, typename... Args, template <typename...> typename F>
  static void init_func_view(F<R(Args...)>& call, t_outlet& outlet)
  {
    using func_t = R(Args...);
    using refl = avnd::function_reflection_t<func_t>;

    call.context = &outlet;
    call.function = [](void* ptr, Args... args) {
      t_outlet* p = static_cast<t_outlet*>(ptr);
      do_process_outlet{p}(std::forward<Args>(args)...);
    };
  }

  template <typename R, typename... Args, template <typename...> typename F>
  static void init_func(F<R(Args...)>& call, t_outlet& outlet)
  {
    using func_t = R(Args...);
    using refl = avnd::function_reflection_t<func_t>;

    call = [&outlet](void* ptr, Args... args)  {
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
    for(auto name : std::ranges::reverse_view(names)) {
        outlets[--out_k] = static_cast<t_outlet*>(outlet_new(&x_obj, name->s_name));
    }

    out_k = 0;
    avnd::output_introspection<T>::for_all(
        avnd::get_outputs<T>(implementation), [this, &out_k, &x_obj](auto& ctl) {
          this->setup(ctl, *outlets[out_k]);
          out_k++;
        });
    }
  }

  std::array<t_outlet*, avnd::output_introspection<T>::size> outlets;
};

}
