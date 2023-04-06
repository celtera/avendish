#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vintage/helpers.hpp>
#include <avnd/binding/vintage/vintage.hpp>

namespace vintage
{

template <typename T>
concept synth_voice = requires(T t) {
                        t.frequency;
                        t.volume;
                        t.elapsed;
                        t.release_frame;
                        t.recycle;
                      };

template <typename T>
struct PolyphonicSynthesizer : vintage::Effect
{
  static_assert(
      synth_voice<typename T::voice>,
      "T does not implement a correct synth voice system");

  T implementation;
  vintage::HostCallback master{};

  SynthControls<T> controls;
  Processor<T> processor;
  Programs<T> programs;

  explicit PolyphonicSynthesizer(vintage::HostCallback master)
      : master{master}
  {
    Effect::dispatcher = [](Effect* effect, int32_t opcode, int32_t index,
                            intptr_t value, void* ptr, float opt) -> intptr_t {
      auto& self = *static_cast<PolyphonicSynthesizer*>(effect);
      auto code = static_cast<EffectOpcodes>(opcode);

      if(code == EffectOpcodes::Close)
      {
        delete &self;
        return 1;
      }

      if constexpr(requires {
                     self.implementation.dispatch(nullptr, 0, 0, 0, nullptr, 0.f);
                   })
      {
        return self.implementation.dispatch(self, code, index, value, ptr, opt);
      }
      else
      {
        return default_dispatch(self, code, index, value, ptr, opt);
      }
    };

    processor.init(*this);
    controls.init(*this);
    programs.init(*this);

    Effect::numInputs = T::channels;
    Effect::numOutputs = T::channels;
    Effect::numParams = Controls<T>::parameter_count + 3;

    Effect::flags = EffectFlags::CanReplacing | EffectFlags::CanDoubleReplacing
                    | EffectFlags::IsSynth;
    Effect::ioRatio = 1.;
    Effect::object = nullptr;
    Effect::user = nullptr;
    Effect::uniqueID = T::unique_id;
    Effect::version = 1;

    controls.read(implementation);

    voices.reserve(127);
    release_voices.reserve(127);
  }

  intptr_t request(HostOpcodes opcode, int a, int b, void* c, float d)
  {
    return this->master(this, static_cast<int32_t>(opcode), a, b, c, d);
  }

  void note_on(int32_t note, int32_t velocity)
  {
    voices.push_back({.note = float(note), .velocity = float(velocity), .detune = 0.0f});
    float unison = this->controls.unison_voices * 20.0;
    float detune = this->controls.unison_detune;
    float vol = this->controls.unison_volume;
    for(float i = -unison; i <= unison; i += 2.f)
    {
      voices.push_back(
          {.note = float(note),
           .velocity = velocity * vol,
           .detune = i * (1.f + detune),
           .pan = (int(i / 2) % 2) ? -1.f : 1.f});
    }
  }

  void note_off(int32_t note, int32_t velocity)
  {
    for(auto it = voices.begin(); it != voices.end();)
    {
      if(it->note == note)
      {
        it->implementation.release_frame = it->implementation.elapsed;
        release_voices.push_back(*it);
        it = voices.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  void bend(int32_t bend)
  {
    for(auto& voice : voices)
    {
      voice.bend = bend / 100.;
    }
    for(auto& voice : release_voices)
    {
      voice.bend = bend / 100.;
    }
  }

  void midi_input(const vintage::MidiEvent& e)
  {
    switch(e.midiData[0] & 0xF0)
    {
      case 0x80: // Note off
      {
        note_off(e.midiData[1] & 0x7F, e.midiData[2] & 0x7F);
        break;
      }

      case 0x90: // Note on
      {
        if(int velocity = e.midiData[2] & 0x7F; velocity > 0)
        {
          note_on(e.midiData[1] & 0x7F, velocity);
        }
        else
        {
          note_off(e.midiData[1] & 0x7F, 0);
        }
        break;
      }

      case 0xE0: // Pitch bend
      {
        int32_t lsb = (e.midiData[1] & 0x7F);
        int32_t msb = (e.midiData[2] & 0x7F);
        bend((msb << 7) + lsb - 0x2000);
        break;
      }
    }
  }

  struct voice
  {
    float note{};
    float velocity{};
    float detune{};
    float bend{};
    float pan{};

    typename T::voice implementation;

    template <typename sample_t>
    void process(PolyphonicSynthesizer& self, sample_t** outputs, int32_t frames)
    {
      implementation.frequency
          = 440. * std::pow(2.0, (note - 69) / 12.0) + detune + bend;
      implementation.volume = velocity / 127.;

      if constexpr(std::size(decltype(implementation.pan){}) == 2)
      {
        implementation.pan[0] = pan == -1.f ? 1. : 0.;
        implementation.pan[1] = pan == 1.f ? 1. : 0.;
      }

      implementation.process(self.implementation, outputs, frames);
    }
  };

  void process(
      avnd::floating_point auto** inputs, avnd::floating_point auto** outputs,
      int32_t frames)
  {
    // Check if processing is to be bypassed
    if constexpr(requires { implementation.bypass; })
    {
      if(implementation.bypass)
        return;
    }

    // Before processing starts, we copy all our atomics back into the struct
    controls.write(implementation);

    // Clear buffer
    for(int32_t c = 0; c < implementation.channels; c++)
      for(int32_t i = 0; i < frames; i++)
        outputs[c][i] = 0.0;

    // Process voices
    for(auto& voice : voices)
    {
      voice.process(*this, outputs, frames);
    }

    // Process voices that were note'off'd in order to cleanly fade out
    for(auto it = release_voices.begin(); it != release_voices.end();)
    {
      auto& voice = *it;
      voice.process(*this, outputs, frames);
      if(voice.implementation.recycle)
        it = release_voices.erase(it);
      else
        ++it;
    }

    // Post-processing
    if constexpr(effect_processor<float, T> || effect_processor<double, T>)
    {
      implementation.process(inputs, outputs, frames);
    }
  }

  std::vector<voice> voices;
  std::vector<voice> release_voices;
};
}

#define VINTAGE_DEFINE_SYNTH(EffectMainClass)                        \
  extern "C" VINTAGE_EXPORTED_SYMBOL vintage::Effect* VSTPluginMain( \
      vintage::HostCallback cb)                                      \
  {                                                                  \
    return new vintage::PolyphonicSynthesizer<EffectMainClass>{cb};  \
  }
