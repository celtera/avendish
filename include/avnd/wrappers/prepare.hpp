#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/for_nth.hpp>
#include <avnd/common/function_reflection.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/concepts/worker.hpp>
#include <avnd/wrappers/effect_container.hpp>

#include <functional>

namespace avnd
{
struct process_setup
{
  int input_channels{};
  int output_channels{};

  // also known as buffer size
  int frames_per_buffer{};

  // sample rate
  double rate{};

  // Unique instance for the whole "model" object.
  // e.g. a plug-in instantiated twice in the same DAW will get two
  // different instance numbers
  uint64_t instance{};

  double samples_to_model{1.};

  double model_to_samples{1.};
};

// Host-callback members on the effect (worker.request, request_channels...)
// must never be left as empty std::functions: calling one terminates the
// process under -fno-exceptions. Bindings without a richer implementation
// call this to install safe fallbacks; bindings that support a concept
// natively wire their own handler afterwards.
template <typename T>
void wire_fallback_callbacks(avnd::effect_container<T>& implementation)
{
  // Workers offload a job to the host: run it inline (same result, no
  // background thread).
  if constexpr(avnd::has_worker<T>)
  {
    for(auto& impl : implementation.effects())
    {
      using worker_t = std::decay_t<decltype(impl.worker)>;
      impl.worker.request = [](auto&&... args) {
        worker_t::work(std::forward<decltype(args)>(args)...);
      };
    }
  }

  // Runtime channel-count requests (variable_audio_bus): most plug-in APIs
  // only renegotiate layouts across a restart; accept and ignore.
  if constexpr(avnd::has_inputs<T> || avnd::has_outputs<T>)
  {
    for(auto state : implementation.full_state())
    {
      auto wire = [](auto& port) {
        if constexpr(requires {
                       port.request_channels = std::function<void(int)>{};
                     })
          port.request_channels = [](int) {};
      };
      if constexpr(avnd::has_inputs<T>)
        avnd::for_each_field_ref(state.inputs, wire);
      if constexpr(avnd::has_outputs<T>)
        avnd::for_each_field_ref(state.outputs, wire);
    }
  }
}

template <typename T>
void prepare(avnd::effect_container<T>& implementation, process_setup setup)
{
  if constexpr(avnd::can_prepare<T>)
  {
    if constexpr(avnd::function_reflection<&T::prepare>::count == 1)
    {
      using prepare_type = avnd::first_argument<&T::prepare>;
      prepare_type t;

      // C++20:
      // using "requires" to check easily which members are available
      // in a structure
      if_possible(t.input_channels = setup.input_channels);
      if_possible(t.output_channels = setup.output_channels);
      if_possible(t.channels = setup.output_channels);
      if_possible(t.frames = setup.frames_per_buffer);
      if_possible(t.rate = setup.rate);
      if_possible(t.instance = setup.instance);
      if_possible(t.samples_to_model = setup.samples_to_model);
      if_possible(t.model_to_samples = setup.model_to_samples);

      // Coroutines get used here.
      int k = 0;
      for(auto& eff : implementation.effects())
      {
        // Individual instance for the polyphonic object case
        if_possible(t.subinstance = k);
        k++;
        eff.prepare(t);
      }
    }
    else
    {
      for(auto& eff : implementation.effects())
        eff.prepare();
    }
  }
}

template <typename T>
void prepare(T& implementation, process_setup setup)
{
  if constexpr(avnd::can_prepare<T>)
  {
    if constexpr(avnd::function_reflection<&T::prepare>::count == 1)
    {
      using prepare_type = avnd::first_argument<&T::prepare>;
      prepare_type t;

      // C++20:
      // using "requires" to check easily which members are available
      // in a structure
      if_possible(t.input_channels = setup.input_channels);
      if_possible(t.output_channels = setup.output_channels);
      if_possible(t.channels = setup.output_channels);
      if_possible(t.frames = setup.frames_per_buffer);
      if_possible(t.rate = setup.rate);
      if_possible(t.instance = setup.instance);
      if_possible(t.samples_to_model = setup.samples_to_model);
      if_possible(t.model_to_samples = setup.model_to_samples);

      implementation.prepare(t);
    }
    else
    {
      implementation.prepare();
    }
  }
}

}
