#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

namespace avnd
{

template <typename T>
concept single_audio_bus_poly_port_processor
    = polyphonic_audio_processor<T> &&(
          (poly_array_port_based<float, T>) || (poly_array_port_based<double, T>))
      && (audio_bus_input_introspection<T>::size == 1
          && audio_bus_output_introspection<T>::size == 1
          && dynamic_poly_audio_port<typename audio_bus_input_introspection<T>::template nth_element<
              0>> && dynamic_poly_audio_port<typename audio_bus_output_introspection<T>::template nth_element<0>>);


template <typename T>
concept single_audio_input_poly_port_processor
    = polyphonic_audio_processor<T> &&(
          (poly_array_port_based<float, T>) || (poly_array_port_based<double, T>))
      && (audio_bus_input_introspection<T>::size == 1
          && audio_bus_output_introspection<T>::size == 0
          && dynamic_poly_audio_port<typename audio_bus_input_introspection<T>::template nth_element<0>>);

template <typename FP, typename T>
struct needs_storage : std::false_type
{
  using needed_storage_t = void;
};

// If our processor supports doubles, and does not support float
// And our host wants to send float, then we have to allocate a buffer
// of doubles
template <typename T>
requires(avnd::double_processor<T> && !avnd::float_processor<T>) struct needs_storage<
    float, T> : std::true_type
{
  using needed_storage_t = double;
};

template <typename T>
requires(!avnd::double_processor<T> && avnd::float_processor<T>) struct needs_storage<
    double, T> : std::true_type
{
  using needed_storage_t = float;
};

template <typename FP, typename T>
using buffer_type = std::conditional_t<
    needs_storage<FP, T>::value,
    std::vector<typename needs_storage<FP, T>::needed_storage_t> // TODO aligned alloc
    ,
    dummy>;

template <typename T, typename Tick>
requires requires(Tick t)
{
  t.frames();
}
auto current_tick(avnd::effect_container<T>& implementation, const Tick& tick_data)
{
  // Nice little C++20 goodie: remove_cvref_t
  // unused in the end using tick_setup_t = std::remove_cvref_t<avnd::second_argument<&T::operator()>>;
  if constexpr(has_tick<T>)
  {
    using tick_t = typename T::tick;
    static_assert(std::is_aggregate_v<tick_t>);

    tick_t t{};
    if_possible(t.frames = tick_data.frames());
    if_possible(t.speed = tick_data.speed());
    if_possible(t.tempo = tick_data.tempo());
    if constexpr(
        requires { t.signature; } && requires { tick_data.signature(); })
    {
      auto [num, denom] = tick_data.signature();
      t.signature = {num, denom};
    }
    if_possible(t.position_in_frames = tick_data.position_in_frames());
    if_possible(t.position_in_seconds = tick_data.position_in_seconds());
    if_possible(t.position_in_nanoseconds = tick_data.position_in_nanoseconds());
    if_possible(t.start_position_in_quarters = tick_data.start_position_in_quarters());
    if_possible(t.end_position_in_quarters = tick_data.end_position_in_quarters());

    // Position of the last bar relative to start in quarter notes
    if_possible(t.bar_at_start = tick_data.bar_at_start());

    // Position of the last bar relative to end in quarter notes
    if_possible(t.bar_at_end = tick_data.bar_at_end());

    return t;
  }
}

inline constexpr auto get_frames(std::integral auto v)
{
  return v;
}

template <typename Tick>
requires requires(Tick t)
{
  t.frames();
}
inline constexpr auto get_frames(const Tick& v)
{
  return v.frames();
}

template <typename T>
inline constexpr auto
get_tick_or_frames(avnd::effect_container<T>& implementation, std::integral auto v)
{
  if constexpr(avnd::has_tick<T>)
  {
    using tick_t = typename T::tick;
    static_assert(std::is_aggregate_v<tick_t>);
    tick_t t{};
    if_possible(t.frames = v);
    return t;
  }
  else
  {
    return v;
  }
}
template <typename T, typename Tick>
requires requires(Tick t)
{
  t.frames();
}
inline constexpr auto
get_tick_or_frames(avnd::effect_container<T>& implementation, const Tick& v)
{
  if constexpr(avnd::has_tick<T>)
    return current_tick(implementation, v);
  else
    return v.frames();
}

// This function goes from a host-provided tick to what the plugin expects
template <typename T, typename Tick>
void invoke_effect(avnd::effect_container<T>& implementation, const Tick& t)
{
  // clang-format off
  if constexpr(std::is_integral_v<Tick>)
  {
    static_assert(!has_tick<T>);
    if_possible(implementation.effect(t))
    else if_possible(implementation.effect());
  }
  else
  {
    if constexpr (has_tick<T>)
    {
      // Do the process call
      if_possible(implementation.effect(t))
      else if_possible(implementation.effect(t.frames))
      else if_possible(implementation.effect(t.frames, t))
      else if_possible(implementation.effect());
    }
    else
    {
      if_possible(implementation.effect(t.frames))
      else if_possible(implementation.effect());
    }
  }
  // clang-format on
}

}
