#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/parameter.hpp>
#include <avnd/binding/pd/helpers.hpp>
#include <avnd/introspection/output.hpp>

namespace pd
{

template <typename T>
void value_to_pd(t_outlet* outlet, T v)
{
  constexpr int sz = boost::pfr::tuple_size_v<T>;
  if constexpr (sz == 0)
  {
    outlet_bang(outlet);
  }
  else if constexpr (sz == 2)
  {
    auto [x, y] = v;
    t_atom arg[2]{
      { .a_type = A_FLOAT, .a_w = { .w_float = x } },
      { .a_type = A_FLOAT, .a_w = { .w_float = y } }
    };
    outlet_list(outlet, &s_list, 2, arg);
  }
  else if constexpr (sz == 3)
  {
    auto [x, y, z] = v;
    t_atom arg[3]{
      { .a_type = A_FLOAT, .a_w = { .w_float = x } },
      { .a_type = A_FLOAT, .a_w = { .w_float = y } },
      { .a_type = A_FLOAT, .a_w = { .w_float = z } }
    };
    outlet_list(outlet, &s_list, 3, arg);
  }
  else if constexpr (sz == 4)
  {
    auto [x, y, z, w] = v;
    t_atom arg[4]{
      { .a_type = A_FLOAT, .a_w = { .w_float = x } },
      { .a_type = A_FLOAT, .a_w = { .w_float = y } },
      { .a_type = A_FLOAT, .a_w = { .w_float = z } },
      { .a_type = A_FLOAT, .a_w = { .w_float = w } },
    };
    outlet_list(outlet, &s_list, 4, arg);
  }
  else
  {
    static_assert(std::is_void_v<T>, "unsupported case");
  }
}

inline void value_to_pd(t_outlet* outlet, int v) noexcept
{ outlet_float(outlet, v); }
inline void value_to_pd(t_outlet* outlet, float v) noexcept
{ outlet_float(outlet, v); }
inline void value_to_pd(t_outlet* outlet, double v) noexcept
{ outlet_float(outlet, v); }
inline void value_to_pd(t_outlet* outlet, bool v) noexcept
{ outlet_float(outlet, v ? 1.f : 0.f); }
inline void value_to_pd(t_outlet* outlet, const char* v) noexcept
{ outlet_symbol(outlet, gensym(v)); }
inline void value_to_pd(t_outlet* outlet, std::string_view v) noexcept
{ outlet_symbol(outlet, gensym(v.data())); }
inline void value_to_pd(t_outlet* outlet, const std::string& v) noexcept
{ outlet_symbol(outlet, gensym(v.c_str())); }

template<typename T>
struct value_writer
{
    T& self;

    template <avnd::parameter Field, std::size_t Idx>
    requires(!avnd::sample_accurate_parameter<Field>) void
    operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
    {
      value_to_pd(port, ctrl.value);
    }

    template <avnd::linear_sample_accurate_parameter Field, std::size_t Idx>
    void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
    {
      auto& buffers = self.control_buffers.linear_inputs;
      // Idx is the index of the port in the complete input array.
      // We need to map it to the linear input index.
      using processor_type = typename T::processor_type;
      using lin_out = avnd::linear_timed_parameter_output_introspection<processor_type>;
      using indices = typename lin_out::indices_n;
      constexpr int storage_index = avnd::index_of_element<Idx>(indices{});

      auto& buffer = std::get<storage_index>(buffers);

      for (int i = 0, N = self.buffer_size; i < N; i++)
      {
        if (buffer[i])
        {
          value_to_pd(port, *buffer[i]);
          buffer[i] = {};
        }
      }
    }

    template <avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
    void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>) const noexcept
    {
      for (auto& [timestamp, val] : ctrl.values)
      {
        value_to_pd(port, val);
      }
      ctrl.values.clear();
    }

    // does not make sense as output, only as input
    template <avnd::span_sample_accurate_parameter Field, std::size_t Idx>
    void operator()(Field& ctrl, t_outlet* port, avnd::num<Idx>)
        const noexcept = delete;

    void operator()(auto&&...) const noexcept { }
};



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
    if constexpr (refl::count == 0)
    {
      call.function = [](void* ptr)
      {
        t_outlet* p = static_cast<t_outlet*>(ptr);
        outlet_bang(p);
      };
    }
    else if constexpr (refl::count == 1)
    {
      if constexpr (std::is_same_v<func_t, void(float)>)
      {
        call.function = [](void* ptr, float f)
        {
          t_outlet* p = static_cast<t_outlet*>(ptr);
          outlet_float(p, f);
        };
      }
      else if constexpr (std::is_same_v<func_t, void(const char*)>)
      {
        call.function = [](void* ptr, const char* f)
        {
          t_outlet* p = static_cast<t_outlet*>(ptr);
          outlet_symbol(p, gensym(f));
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

    if constexpr (refl::count == 0)
    {
      call = [&outlet] { outlet_bang(&outlet); };
    }
    else if constexpr (refl::count == 1)
    {
      if constexpr (std::is_same_v<func_t, void(float)>)
      {
        call = [&outlet](float f) { outlet_float(&outlet, f); };
      }
      else if constexpr (std::is_same_v<func_t, void(const char*)>)
      {
        call = [&outlet](const char* f) { outlet_symbol(&outlet, gensym(f)); };
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
    if constexpr (avnd::function_view_ish<call_type>)
    {
      init_func_view(out.call, outlet);
    }
    else if constexpr (avnd::function_ish<call_type>)
    {
      init_func(out.call, outlet);
    }
  }

  template <typename Out>
  void setup(Out& out, t_outlet& outlet)
  {
  }

  template<typename Self>
  void commit(Self& self)
  {
    using info = avnd::output_introspection<T>;
    if constexpr(info::size > 0)
    {
      auto& outs = avnd::get_outputs<T>(self.implementation);
      [&]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        (value_writer<Self>{self}(
           boost::pfr::get<Index>(outs),
           outlets[Index],
           avnd::num<Index>{}),
         ...);
      }
      (typename info::indices_n{});
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
          [this, &out_k, &x_obj](auto& ctl)
          {
            outlets[out_k] = outlet_new(&x_obj, symbol_for_port(ctl));
            this->setup(ctl, *outlets[out_k]);
            out_k++;
          });
    }
  }

  std::array<t_outlet*, avnd::output_introspection<T>::size> outlets;
};

}
