#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>

namespace max
{
template <typename T>
struct outputs
{
  template <avnd::function_view_ish call_type>
  static void init_func_view(call_type& call, t_outlet& outlet)
  {
    using func_t = typename call_type::type;
    using refl = avnd::function_reflection_t<func_t>;

    call.context = &outlet;
    call.function = nullptr;
    if constexpr(refl::count == 0)
    {
      call.function = [](void* ptr) {
        t_outlet* p = static_cast<t_outlet*>(ptr);
        outlet_bang(p);
      };
    }
    else if constexpr(refl::count == 1)
    {
      if constexpr(std::is_same_v<func_t, void(float)>)
      {
        call.function = [](void* ptr, float f) {
          t_outlet* p = static_cast<t_outlet*>(ptr);
          outlet_float(p, f);
        };
      }
      else if constexpr(std::is_same_v<func_t, void(const char*)>)
      {
        call.function = [](void* ptr, const char* f) {
          t_outlet* p = static_cast<t_outlet*>(ptr);
          outlet_anything(p, gensym(f), 0, nullptr);
        };
      }
    }
    else
    {
      AVND_STATIC_TODO(call_type);
    }
  }

  template <typename R, typename... Args, template <typename...> typename F>
  static void init_func(F<R(Args...)>& call, t_outlet& outlet)
  {
    using func_t = R(Args...);
    using refl = avnd::function_reflection_t<func_t>;

    if constexpr(refl::count == 0)
    {
      call = [&outlet] { outlet_bang(&outlet); };
    }
    else if constexpr(refl::count == 1)
    {
      if constexpr(std::is_same_v<func_t, void(float)>)
      {
        call = [&outlet](float f) { outlet_float(&outlet, f); };
      }
      else if constexpr(std::is_same_v<func_t, void(const char*)>)
      {
        call = [&outlet](const char* f) {
          outlet_anything(&outlet, gensym(f), 0, nullptr);
        };
      }
    }
    else
    {
      AVND_STATIC_TODO(F<R(Args...)>);
    }
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
    int out_k = 0;
    avnd::output_introspection<T>::for_all(
        avnd::get_outputs<T>(implementation), [this, &out_k, &x_obj](auto& ctl) {
          auto ptr = outlet_new(&x_obj, symbol_for_port(ctl)->s_name);
          outlets[out_k] = static_cast<t_outlet*>(ptr);
          this->setup(ctl, *outlets[out_k]);
          out_k++;
        });
  }

  std::array<t_outlet*, avnd::output_introspection<T>::size> outlets;
};

}
