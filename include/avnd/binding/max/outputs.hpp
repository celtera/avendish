#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/to_atoms.hpp>

namespace max
{
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

  void operator()(const avnd::map_ish auto& v) const noexcept
  {
    /*
    to_dict l;
    l(v);
    outlet_list(p, nullptr, l.atoms.size(), l.atoms.data());
    */
  }
};


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
