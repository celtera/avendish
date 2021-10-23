#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/controls.hpp>
#include <vintage/helpers.hpp>
#include <vintage/vintage.hpp>

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
  using inputs_info_t = float_parameter_input_introspection<T>;
  static const constexpr int32_t parameter_count = inputs_info_t::size;
  std::atomic<float> parameters[parameter_count];

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
      using inputs_t = std::remove_reference_t<decltype(source)>;
      (sink[Index].store(
           avnd::map_control_to_01(
               boost::pfr::get<inputs_info_t::index_map[Index]>(source)),
           std::memory_order_relaxed),
       ...);
    }
    (std::make_index_sequence<parameter_count>());

    std::atomic_thread_fence(std::memory_order_release);
  }

  void read(const avnd::effect_container<T>& implementation)
  {
    read(implementation.inputs());
  }

  void write(avnd::effect_container<T>& implementation)
  {
    std::atomic_thread_fence(std::memory_order_acquire);

    [ this, &implementation ]<std::size_t... Index>(
        std::integer_sequence<std::size_t, Index...>)
    {
      auto& source = this->parameters;
      auto& sink = implementation.inputs();
      using inputs_t = std::remove_reference_t<decltype(sink)>;
      ((avnd::map_control_from_01(
           boost::pfr::get<inputs_info_t::index_map[Index]>(sink),
           source[Index].load(std::memory_order_relaxed))),
       ...);
    }
    (std::make_index_sequence<parameter_count>());
  }

  template <typename Effect_T>
  void name(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    for_nth_parameter(
        self,
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
    for_nth_parameter(
        self,
        index,
        [ptr](const auto& param)
        {
          if constexpr (requires
                        { param.display((char*)nullptr, param.value); })
          {
            param.display(reinterpret_cast<char*>(ptr), param.value);
          }
          else if constexpr (requires { param.display((char*)nullptr); })
          {
            param.display(reinterpret_cast<char*>(ptr));
          }
          else if constexpr (requires { param.display(); })
          {
            vintage::param_display{param.display()}.copy_to(ptr);
          }
          else
          {
            auto cstr = reinterpret_cast<char*>(ptr);
#if __has_include(<fmt/format.h>) && 0
            if constexpr (std::is_same_v<decltype(param.value), float>)
              fmt::format_to(cstr, "{:.2f}", param.value);
            else
              fmt::format_to(cstr, "{}", param.value);
#else
            if constexpr (std::is_same_v<decltype(param.value), float>)
              snprintf(cstr, 16, "%.2f", param.value);
            else if constexpr (std::is_same_v<decltype(param.value), int>)
              snprintf(cstr, 16, "%d", param.value);
            else if constexpr (std::is_same_v<
                                   decltype(param.value),
                                   const char*>)
              snprintf(cstr, 16, "%s", param.value);
            else if constexpr (std::is_same_v<
                                   decltype(param.value),
                                   std::string>)
              snprintf(cstr, 16, "%s", param.value.data());
#endif
          }
        });
  }

  template <typename Effect_T>
  void label(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    for_nth_parameter(
        self,
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
