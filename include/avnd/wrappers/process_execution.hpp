#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include <type_traits>

namespace avnd
{

template <typename T>
concept single_audio_bus_poly_port_processor
    = polyphonic_audio_processor<
          T> && ((poly_array_port_based<float, T>) || (poly_array_port_based<double, T>))
      && (audio_bus_input_introspection<T>::size == 1
          && audio_bus_output_introspection<T>::size == 1
          && dynamic_poly_audio_port<typename audio_bus_input_introspection<T>::template nth_element<
              0>> && dynamic_poly_audio_port<typename audio_bus_output_introspection<T>::template nth_element<0>>);
template <typename FP, typename T>
struct needs_storage : std::false_type
{
  using needed_storage_t = void;
};

// If our processor supports doubles, and does not support float
// And our host wants to send float, then we have to allocate a buffer
// of doubles
template <typename T>
  requires(avnd::double_processor<T> && !avnd::float_processor<T>)
struct needs_storage<float, T> : std::true_type
{
  using needed_storage_t = double;
};

template <typename T>
  requires(!avnd::double_processor<T> && avnd::float_processor<T>)
struct needs_storage<double, T> : std::true_type
{
  using needed_storage_t = float;
};

template <typename FP, typename T>
using buffer_type = std::conditional_t<
    needs_storage<FP, T>::value,
    std::vector<typename needs_storage<FP, T>::needed_storage_t> // TODO aligned alloc
    ,
    dummy>;

// Original idea was to pass everything by arguments here.
// Sadly hard to do as soon as there are references as we cannot do it piecewise
template <typename T>
auto current_tick(avnd::effect_container<T>& implementation)
{
  // Nice little C++20 goodie: remove_cvref_t
  // unused in the end using tick_setup_t = std::remove_cvref_t<avnd::second_argument<&T::operator()>>;
  if constexpr (has_tick<T>)
  {
    using tick_t = typename T::tick;
    static_assert(std::is_aggregate_v<tick_t>);

    // TODO setup shjit

    return tick_t{};
  }
}

template <typename T>
void invoke_effect(avnd::effect_container<T>& implementation, int frames)
{
  // clang-format off
  if constexpr (has_tick<T>)
  {
    // Set-up the "tick" struct
    auto t = current_tick(implementation);
    if_possible(t.frames = frames);

    // Do the process call
    if_possible(implementation.effect(t))
    else if_possible(implementation.effect(frames))
    else if_possible(implementation.effect(frames,t))
    else if_possible(implementation.effect());
  }
  else
  {
    if_possible(implementation.effect(frames))
    else if_possible(implementation.effect());
  }
  // clang-format on
}
}
