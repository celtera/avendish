#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vintage/helpers.hpp>
#include <avnd/binding/vintage/vintage.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/control_display.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#if __has_include(<fmt/format.h>) && !defined(AVND_DISABLE_FMT)
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
  template <typename Effect_T>
  void properties(Effect_T& effect, int index, void* ptr)
  {
  }
};

template <typename T>
  requires(avnd::parameter_input_introspection<T>::size > 0)
struct Controls<T>
{
  using inputs_info_t = avnd::parameter_input_introspection<T>;
  static const constexpr int32_t parameter_count = inputs_info_t::size;
  std::atomic<float> parameters[std::max(parameter_count, 1)];

  template <typename Effect_T>
  void init(Effect_T& effect)
  {
    effect.Effect::setParameter
        = [](Effect* effect, int32_t index, float parameter) noexcept {
      auto& self = *static_cast<Effect_T*>(effect);

      if(index < Controls<T>::parameter_count)
        self.controls.parameters[index].store(parameter, std::memory_order_release);
    };

    effect.Effect::getParameter = [](Effect* effect, int32_t index) noexcept {
      auto& self = *static_cast<Effect_T*>(effect);

      if(index < Controls<T>::parameter_count)
        return self.controls.parameters[index].load(std::memory_order_acquire);
      else
        return 0.f;
    };
  }

  void read(const typename avnd::inputs_type<T>::type& source)
  {
    [ this, &source ]<std::size_t... Index>(std::integer_sequence<std::size_t, Index...>)
    {
      auto& sink = this->parameters;

      auto do_store = [&]<std::size_t N> {
        constexpr auto idx = inputs_info_t::index_map[N];
        if constexpr(requires { avnd::map_control_to_01(avnd::pfr::get<idx>(source)); })
          sink[N].store(
              avnd::map_control_to_01(avnd::pfr::get<idx>(source)),
              std::memory_order_relaxed);
      };
      (do_store.template operator()<Index>(), ...);
    }
    (std::make_index_sequence<parameter_count>());

    std::atomic_thread_fence(std::memory_order_release);
  }

  template <std::size_t Index>
  void write_control(auto& impl, auto& sink)
  {
    using type = typename inputs_info_t::template nth_element<Index>;

    auto& source = this->parameters;
    auto& port = inputs_info_t::template field<Index>(sink);

    if constexpr(requires { avnd::map_control_from_01<type>(0.f); })
      port.value = avnd::map_control_from_01<type>(
          source[Index].load(std::memory_order_relaxed));
    if_possible(port.update(impl));
  }

  void write(avnd::effect_container<T>& implementation)
  {
    std::atomic_thread_fence(std::memory_order_acquire);

    [ this, &implementation ]<std::size_t... Index>(
        std::integer_sequence<std::size_t, Index...>)
    {
      for(auto& state : implementation.full_state())
      {
        (this->write_control<Index>(state.effect, state.inputs), ...);
      }
    }
    (std::make_index_sequence<parameter_count>());
  }

  template <typename Effect_T>
  void name(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    inputs_info_t::for_nth_mapped(
        self.inputs(), index, [ptr]<typename P>(const P& param) {
          if constexpr(requires { P::name(); })
          {
            vintage::name{P::name()}.copy_to(ptr);
          }
          else if constexpr(requires { P::label(); })
          {
            vintage::label{P::label()}.copy_to(ptr);
          }
        });
  }

  template <typename Effect_T>
  void display(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    inputs_info_t::for_nth_mapped(
        self.inputs(), index,
        [ptr]<typename C>(const C& param) {
      avnd::display_control<C>(
          param.value, reinterpret_cast<char*>(ptr), vintage::Constants::ParamStrLen);
        });
  }

  template <typename Effect_T>
  void label(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    inputs_info_t::for_nth_mapped(
        self.inputs(), index, [ptr]<typename P>(const P& param) {
          if constexpr(avnd::has_label<P>)
          {
            vintage::label{avnd::get_label<P>()}.copy_to(ptr);
          }
          else if constexpr(avnd::has_name<P>)
          {
            vintage::label{avnd::get_name<P>()}.copy_to(ptr);
          }
        });
  }

  template <typename Effect_T>
  void properties(Effect_T& effect, int index, void* ptr)
  {
    auto& self = effect.effect;
    auto& props = *(vintage::ParameterProperties*)ptr;

    inputs_info_t::for_nth_mapped(
        self.inputs(), index, [&props, index](const auto& param) {
          props.stepFloat = 0.01;
          props.smallStepFloat = 0.01;
          props.largeStepFloat = 0.01;

          if constexpr(requires { param.label(); })
          {
            vintage::label{param.label()}.copy_to(props.label);
          }
          props.flags = {};
          props.minInteger = 0;
          props.maxInteger = 1;
          props.stepInteger = 1;
          props.largeStepInteger = 1;

          if constexpr(requires { param.shortLabel(); })
          {
            vintage::short_label{param.shortLabel()}.copy_to(props.shortLabel);
          }
          props.displayIndex = index;
          props.category = 0;
          props.numParametersInCategory = 2;
          if constexpr(requires { param.categoryLabel(); })
          {
            vintage::category_label{param.categoryLabel()}.copy_to(props.categoryLabel);
          }
        });
  }
};

}
