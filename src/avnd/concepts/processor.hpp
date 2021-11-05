#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/port.hpp>
#include <common/function_reflection.hpp>

namespace avnd
{
// Has a "struct tick" member that will be passed as argument
// to the main processing function
template <typename T>
concept has_tick = std::is_default_constructible_v<typename T::tick>;


template <typename FP, typename T>
static constexpr int sample_input_port_count = boost::mp11::mp_count_if_q<
    typename inputs_type<T>::tuple,
    is_audio_sample_port_q<FP>>::value;
template <typename FP, typename T>
static constexpr int sample_output_port_count = boost::mp11::mp_count_if_q<
    typename outputs_type<T>::tuple,
    is_audio_sample_port_q<FP>>::value;

template <typename FP, typename T>
static constexpr int mono_sample_array_input_port_count
    = boost::mp11::mp_count_if_q<
        typename inputs_type<T>::tuple,
        is_mono_array_sample_port_q<FP>>::value;
template <typename FP, typename T>
static constexpr int mono_sample_array_output_port_count
    = boost::mp11::mp_count_if_q<
        typename outputs_type<T>::tuple,
        is_mono_array_sample_port_q<FP>>::value;

template <typename FP, typename T>
static constexpr int poly_sample_array_input_port_count
    = boost::mp11::mp_count_if_q<
        typename inputs_type<T>::tuple,
        is_poly_array_sample_port_q<FP>>::value;
template <typename FP, typename T>
static constexpr int poly_sample_array_output_port_count
    = boost::mp11::mp_count_if_q<
        typename outputs_type<T>::tuple,
        is_poly_array_sample_port_q<FP>>::value;

template <typename FP, typename T>
concept sample_port_based
    = (sample_input_port_count<FP, T> > 0
       || sample_output_port_count<FP, T> > 0);
template <typename FP, typename T>
concept mono_array_port_based
    = (mono_sample_array_input_port_count<FP, T> > 0
       || mono_sample_array_output_port_count<FP, T> > 0);
template <typename FP, typename T>
concept poly_array_port_based
    = (poly_sample_array_input_port_count<FP, T> > 0
       || poly_sample_array_output_port_count<FP, T> > 0);

template <typename FP, typename T>
concept effect_is_sane = requires(T t)
{
  (sample_port_based<
       FP,
       T> && !(mono_array_port_based<FP, T> || poly_array_port_based<FP, T>))
      || (mono_array_port_based<
              FP,
              T> && !(sample_port_based<FP, T> || poly_array_port_based<FP, T>))
      || (poly_array_port_based<
              FP,
              T> && !(mono_array_port_based<FP, T> || sample_port_based<FP, T>))
      || (!sample_port_based<
              FP,
              T> && !poly_array_port_based<FP, T> && !mono_array_port_based<FP, T>);
};

/// Definition of what is an audio effect ///
template <typename FP, typename T>
concept monophonic_port_audio_effect
= mono_sample_array_input_port_count<FP, T>
> 0 && mono_sample_array_output_port_count<FP, T> > 0;

template <typename FP, typename T>
concept polyphonic_port_audio_effect
= poly_sample_array_input_port_count<FP, T>
> 0 && poly_sample_array_output_port_count<FP, T> > 0;

template <typename FP, typename T>
concept monophonic_single_port_audio_effect
    = mono_sample_array_input_port_count<FP, T>
== 1 && mono_sample_array_output_port_count<FP, T> == 1;

template <typename FP, typename T>
concept polyphonic_single_port_audio_effect
    = poly_sample_array_input_port_count<FP, T>
== 1 && poly_sample_array_output_port_count<FP, T> == 1;

template <typename FP, typename T>
concept monophonic_arg_audio_effect = requires(T t)
{
  t.operator()((FP*)nullptr, (FP*)nullptr, (int32_t)0);
};

template <typename FP, typename T>
concept polyphonic_arg_audio_effect = requires(T t)
{
  t.operator()((FP**)nullptr, (FP**)nullptr, (int32_t)0);
};

template <auto Func, typename... Args>
concept same_arguments = std::is_same_v<
    typename function_reflection<Func>::arguments,
    boost::mp11::mp_list<Args...>>;

// C++17: is_invocable
template <typename FP, typename T>
concept mono_per_sample_arg_processor = std::is_same_v<typename function_reflection<&T::operator()>::return_type, FP> &&(
    std::
        is_invocable_r_v<
            FP,
            T,
            FP> || std::is_invocable_r_v<FP, T, FP, const typename T::inputs&> || std::is_invocable_r_v<FP, T, FP, typename T::outputs&> || std::is_invocable_r_v<FP, T, FP, const typename T::inputs&, typename T::outputs&> || std::is_invocable_r_v<FP, T, FP, const typename T::tick&> || std::is_invocable_r_v<FP, T, FP, const typename T::inputs&, const typename T::tick&> || std::is_invocable_r_v<FP, T, FP, typename T::outputs&, const typename T::tick&> || std::is_invocable_r_v<FP, T, FP, const typename T::inputs&, typename T::outputs&, const typename T::tick&>);

// TODO use FP
// If a node has exactly one audio in and one audio out it can be considered monophonic
// (and thus be instanced).
// Otherwise, it'll be treated as polyphonic with a single instance.
template <typename FP, typename T>
concept mono_per_sample_port_processor
    = (sample_input_port_count<FP, T> == 1)
      && (sample_output_port_count<FP, T> == 1)
      && (std::
              is_invocable_r_v<
                  void,
                  T> || std::is_invocable_r_v<void, T, const typename T::inputs&> || std::is_invocable_r_v<void, T, typename T::outputs&> || std::is_invocable_r_v<void, T, const typename T::inputs&, typename T::outputs&> || std::is_invocable_r_v<void, T, const typename T::tick&> || std::is_invocable_r_v<void, T, const typename T::inputs&, const typename T::tick&> || std::is_invocable_r_v<void, T, typename T::outputs&, const typename T::tick&> || std::is_invocable_r_v<void, T, const typename T::inputs&, typename T::outputs&, const typename T::tick&>);
template <typename FP, typename T>
concept poly_per_sample_port_processor = ((sample_input_port_count<FP, T> > 1) || (sample_output_port_count<FP, T> > 1)) && (std::is_invocable_r_v<void, T> || std::is_invocable_r_v<void, T, const typename T::inputs&> || std::is_invocable_r_v<void, T, typename T::outputs&> || std::is_invocable_r_v<void, T, const typename T::inputs&, typename T::outputs&> || std::is_invocable_r_v<void, T, const typename T::tick&> || std::is_invocable_r_v<void, T, const typename T::inputs&, const typename T::tick&> || std::is_invocable_r_v<void, T, typename T::outputs&, const typename T::tick&> || std::is_invocable_r_v<void, T, const typename T::inputs&, typename T::outputs&, const typename T::tick&>);

template <typename T>
concept sample_processor
    = mono_per_sample_arg_processor<double, T> || mono_per_sample_port_processor<
        double,
        T> || mono_per_sample_arg_processor<float, T> || mono_per_sample_port_processor<float, T>;

// We have two axes of "adaptation":
// - monophonic / polyphonic
// - float / double
// This gives us the following set of concepts
template <typename FP, typename T>
concept monophonic_processor = effect_is_sane<FP, T> &&(
    monophonic_arg_audio_effect<
        FP,
        T> || mono_array_port_based<FP, T> || mono_per_sample_arg_processor<FP, T> || mono_per_sample_port_processor<FP, T>);

template <typename FP, typename T>
concept polyphonic_processor
    = polyphonic_arg_audio_effect<FP, T> || poly_array_port_based<
        FP,
        T> || poly_per_sample_port_processor<FP, T>;

template <typename T>
concept polyphonic_double_processor = polyphonic_arg_audio_effect<
    double,
    T> || poly_array_port_based<double, T>;

template <typename T>
concept float_processor
    = monophonic_processor<float, T> || polyphonic_processor<float, T>;
template <typename T>
concept double_processor
    = monophonic_processor<double, T> || polyphonic_processor<double, T>;

template <typename T>
concept monophonic_audio_processor
    = monophonic_processor<float, T> || monophonic_processor<double, T>;
template <typename T>
concept polyphonic_audio_processor
    = polyphonic_processor<float, T> || polyphonic_processor<double, T>;


}
