#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/controls.hpp>
#include <avnd/binding/vintage/helpers.hpp>
#include <avnd/binding/vintage/vintage.hpp>

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
#else
#include <cstdlib>
#include <cstring>
#endif

namespace vintage
{

template <typename T>
struct Controls
{
  static const constexpr int32_t parameter_count = 0;
  template <typename Effect_T>
  void init(Effect_T& effect)
  {
  }
  void read(auto&&) { }
  void write(auto&&) { }
  template <typename Effect_T>
  void display(Effect_T& effect, int index, void* ptr)
  {
  }
  template <typename Effect_T>
  void name(Effect_T& effect, int index, void* ptr)
  {
  }
  template <typename Effect_T>
  void label(Effect_T& effect, int index, void* ptr)
  {
  }
};

template <typename T>
requires (float_parameter_input_introspection<T>::size > 0)
struct Controls<T>
{
  using inputs_info_t = float_parameter_input_introspection<T>;
  static const constexpr int32_t parameter_count = inputs_info_t::size;
  std::atomic<float> parameters[std::max(parameter_count, 1)];

  template <typename Effect_T>
  void init(Effect_T& effect)
  {
    effect.Effect::setParameter
        = [](Effect* effect, int32_t index, float parameter) noexcept
    {
      auto& self = *static_cast<Effect_T*>(effect);

      if (index < Controls<T>::parameter_count)
        self.controls.parameters[index].store(
            parameter, std::memory_order_release);
    };

    effect.Effect::getParameter = [](Effect* effect, int32_t index) noexcept
    {
      auto& self = *static_cast<Effect_T*>(effect);

      if (index < Controls<T>::parameter_count)
        return self.controls.parameters[index].load(std::memory_order_acquire);
      else
        return 0.f;
    };
  }

  void read(const typename avnd::inputs_type<T>::type& source)
  {
    [ this, &source ]<std::size_t... Index>(
        std::integer_sequence<std::size_t, Index...>)
    {
      auto& sink = this->parameters;
      (sink[Index].store(
           avnd::map_control_to_01(
               boost::pfr::get<inputs_info_t::index_map[Index]>(source)),
           std::memory_order_relaxed),
       ...);
    }
    (std::make_index_sequence<parameter_count>());

    std::atomic_thread_fence(std::memory_order_release);
  }

  void write(avnd::effect_container<T>& implementation)
  {
    std::atomic_thread_fence(std::memory_order_acquire);

    [ this, &implementation ]<std::size_t... Index>(
        std::integer_sequence<std::size_t, Index...>)
    {
      auto& source = this->parameters;
      auto& sink = implementation.inputs();
      ((inputs_info_t::template get<Index>(sink).value =
          avnd::map_control_from_01<typename inputs_info_t::template nth_element<Index>>(source[Index].load(std::memory_order_relaxed))),
       ...);
    }
    (std::make_index_sequence<parameter_count>());
  }

  template <typename Effect_T>
  void name(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    inputs_info_t::for_nth(
        self.inputs(),
        index,
        [ptr]<typename P>(const P& param)
        {
          if constexpr (requires { P::name(); })
          {
            vintage::name{P::name()}.copy_to(ptr);
          }
        });
  }

  template <typename Effect_T>
  void display(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    inputs_info_t::for_nth(
        self.inputs(),
        index,
        [ptr]<typename C>(const C& param)
        {
          using val_type = std::decay_t<decltype(param.value)>;
          const val_type& value = param.value;
          auto cstr = reinterpret_cast<char*>(ptr);

          if constexpr (requires
                        { param.display(cstr, value); })
          {
            param.display(cstr, value);
          }
          else if constexpr (requires { param.display(cstr); })
          {
            param.display(cstr);
          }
          else if constexpr (requires { param.display(); })
          {
            vintage::param_display{param.display()}.copy_to(ptr);
          }
          else
          {
#if __has_include(<fmt/format.h>)
            if constexpr (std::floating_point<val_type>)
            { fmt::format_to(cstr, "{:.2f}", value); }
            else if constexpr (std::is_enum_v<val_type>)
            {
              const int enum_index = static_cast<int>(value);
              if(enum_index >= 0 && enum_index < C::choices().size())
                fmt::format_to(cstr, "{}", C::choices()[enum_index]);
              else
                fmt::format_to(cstr, "{}", enum_index);
            }
            else
            { fmt::format_to(cstr, "{}", value); }
#else
            if constexpr (std::floating_point<val_type>)
              snprintf(cstr, 16, "%.2f", value);
            else if constexpr (std::is_same_v<val_type, int>)
              snprintf(cstr, 16, "%d", value);
            else if constexpr (std::is_same_v<val_type, bool>)
              snprintf(cstr, 16, value ? "true" : "false");
            else if constexpr (std::is_same_v<val_type, const char*>)
              snprintf(cstr, 16, "%s", value);
            else if constexpr (std::is_same_v<val_type, std::string>)
              snprintf(cstr, 16, "%s", value.data());
            else if constexpr (std::is_enum_v<val_type>)
            {
              const int enum_index = static_cast<int>(value);
              if(enum_index >= 0 && enum_index < C::choices().size())
                snprintf(cstr, 16, "%s", C::choices()[enum_index]);
              else
                snprintf(cstr, 16, "%d", enum_index);
            }
#endif
          }
        });
  }

  template <typename Effect_T>
  void label(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    inputs_info_t::for_nth(
        self.inputs(),
        index,
        [ptr](const auto& param)
        {
          if constexpr (requires { param.label(); })
          {
            vintage::label{param.label()}.copy_to(ptr);
          }
        });
  }
};

}
