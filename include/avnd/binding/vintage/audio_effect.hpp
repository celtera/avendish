#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/channels_introspection.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/common/export.hpp>
#include <avnd/binding/vintage/atomic_controls.hpp>
#include <avnd/binding/vintage/dispatch.hpp>
#include <avnd/binding/vintage/helpers.hpp>
#include <avnd/binding/vintage/midi_processor.hpp>
#include <avnd/binding/vintage/processor_setup.hpp>
#include <avnd/binding/vintage/programs.hpp>
#include <avnd/binding/vintage/vintage.hpp>

namespace vintage
{
static constexpr int32_t hash_uuid(std::string_view uuid)
{
  int32_t res = 0;
  int k = 0;
  for (std::size_t i = 0; i < uuid.size(); i++)
  {
    res += unsigned(uuid[i]) << (k * 8);
    k = (k + 1) % 4;
  }
  return res;
}
template <typename T>
struct SimpleAudioEffect : vintage::Effect
{
  using effect_type = T;
  avnd::effect_container<T> effect;
  vintage::HostCallback master{};

  [[no_unique_address]] Controls<T> controls;

  [[no_unique_address]] ProcessorSetup processorSetup;

  [[no_unique_address]] process_adapter<T> processor;

  [[no_unique_address]] programs_setup programs;

  [[no_unique_address]] midi_processor<T> midi;

  float sample_rate{44100.};
  int buffer_size{512};
  vintage::ProcessPrecision precision
      = avnd::double_processor<T> ? vintage::ProcessPrecision::Double
                                  : vintage::ProcessPrecision::Single;

  int current_program = 0;

  explicit SimpleAudioEffect(vintage::HostCallback master)
      : master{master}
  {
    /// Initialize the Effect struct
    Effect::dispatcher = host_dispatcher;

    Effect::numInputs = avnd::input_channels<T>(2);
    Effect::numOutputs = avnd::output_channels<T>(2);

    Effect::numParams = Controls<T>::parameter_count;

    Effect::flags |= EffectFlags::CanReplacing;
    if constexpr (avnd::double_processor<T>)
      Effect::flags |= EffectFlags::CanDoubleReplacing;
    if constexpr (avnd::midi_input_introspection<T>::size > 0)
      Effect::flags |= EffectFlags::IsSynth;

    Effect::ioRatio = 1.;
    Effect::object = this;
    Effect::user = this;

    if constexpr (requires { T::unique_id(); })
      Effect::uniqueID = T::unique_id();
    else if constexpr (requires { T::uuid(); })
      Effect::uniqueID = hash_uuid(T::uuid());
    else
      Effect::uniqueID = 0xBADBAD;

    if constexpr (requires { T::version(); })
      Effect::version = T::version();
    else
      Effect::version = 1;

    /// Setup all our modules
    processorSetup.init(*this);
    controls.init(*this);
    programs.init(*this);
    effect.init_channels(Effect::numInputs, Effect::numOutputs);

    /// Initialize with the required params
    sample_rate = request(HostOpcodes::GetSampleRate, 0, 0, nullptr, 0.f);
    buffer_size = request(HostOpcodes::GetBlockSize, 0, 0, nullptr, 0.f);

    /// Read the initial state of the controls
    if constexpr (avnd::has_inputs<T>)
    {
      // First the default value
      avnd::init_controls(effect.inputs());

      // Then the preset
      controls.read(effect.inputs());
    }
  }

  // effMainsChanged lifecycle
  void start()
  {
    // Allocate buffers, setup everything
    avnd::process_setup setup_info{
        .input_channels = Effect::numInputs,
        .output_channels = Effect::numOutputs,
        .frames_per_buffer = buffer_size,
        .rate = sample_rate};

    // Setup buffers for eventual float <-> double conversion
    // We always do so for float, as the plug-in API requires the existence of this
    processor.allocate_buffers(setup_info, float{});
    if constexpr (avnd::double_processor<T>)
      processor.allocate_buffers(setup_info, double{});

    // Setup buffers for storing MIDI messages
    if constexpr (midi_input_introspection<T>::size > 0)
    {
      using i_info = avnd::midi_input_introspection<T>;
      auto& in_port = boost::pfr::get<i_info::index_map[0]>(effect.inputs());

      midi.reserve_space(in_port, buffer_size);
    }

    // Effect-specific preparation
    avnd::prepare(effect, setup_info);
  }

  ~SimpleAudioEffect() { }

  void stop() { }

  // Request a message to the host
  intptr_t request(HostOpcodes opcode, int a, int b, void* c, float d)
  {
    return this->master(this, static_cast<int32_t>(opcode), a, b, c, d);
  }

  void process(
      std::floating_point auto** inputs,
      std::floating_point auto** outputs,
      int32_t sampleFrames)
  {
    // Check if processing is to be bypassed
    if constexpr (avnd::can_bypass<T>)
    {
      for (auto& impl : effect.effects())
        if (impl.bypass)
          return;
    }

    // Before processing starts, we copy all our atomics back into the struct
    controls.write(effect);

    // Actual processing
    processor.process(
        effect,
        std::span{inputs, std::size_t(this->Effect::numInputs)},
        std::span{outputs, std::size_t(this->Effect::numOutputs)},
        sampleFrames);

    // Clear our midi ports
    if constexpr (midi_input_introspection<T>::size > 0)
    {
      using i_info = avnd::midi_input_introspection<T>;
      auto& in_port = boost::pfr::get<i_info::index_map[0]>(effect.inputs());

      midi.clear(in_port);
    }
  }

  void event_input(const vintage::Events* evs)
  {
    if constexpr (midi_input_introspection<T>::size > 0)
    {
      using i_info = avnd::midi_input_introspection<T>;
      auto& in_port = boost::pfr::get<i_info::index_map[0]>(effect.inputs());

      // In case we need to allocate more storage:
      const int n = evs->numEvents;
      midi.reserve_space(in_port, n);

      for (int32_t i = 0; i < n; i++)
      {
        const auto* ev = evs->events[i];
        switch (ev->type)
        {
          case vintage::EventTypes::Midi:
          {
            auto& event = *reinterpret_cast<const vintage::MidiEvent*>(ev);
            midi.add_message(in_port, event);
            break;
          }
          default:
            break;
        }
      }
    }
  }

  // Process messages from the host
  static std::intptr_t host_dispatcher(
      Effect* effect,
      int32_t opcode,
      int32_t index,
      intptr_t value,
      void* ptr,
      float opt)
  {
    auto& self = *static_cast<SimpleAudioEffect*>(effect);
    auto code = static_cast<EffectOpcodes>(opcode);

    if (code == EffectOpcodes::Close)
    {
      delete &self;
      return 1;
    }

    // If our plug-in has a custom dispatching implementation, then we can use it
    if constexpr (vintage::can_dispatch<T>)
    {
      return self.implementation.dispatch(self, code, index, value, ptr, opt);
    }
    else
    {
      return default_dispatch(self, code, index, value, ptr, opt);
    }
  }
};
}
