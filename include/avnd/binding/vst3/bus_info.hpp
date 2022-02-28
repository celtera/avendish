#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vst3/helpers.hpp>
#include <avnd/binding/vst3/metadata.hpp>
#include <avnd/wrappers/channels_introspection.hpp>
#include <avnd/wrappers/input_introspection.hpp>
#include <avnd/wrappers/output_introspection.hpp>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/vstspeaker.h>

namespace stv3
{
template <typename T>
struct event_bus_info
{
  static constexpr int inputCount() noexcept
  {
    return avnd::midi_input_introspection<T>::size;
  }
  static constexpr int outputCount() noexcept
  {
    return avnd::midi_output_introspection<T>::size;
  }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    using namespace Steinberg::Vst;
    static const constexpr int input_event_busses
        = avnd::midi_input_introspection<T>::size;

    if (index < input_event_busses)
    {
      info.mediaType = MediaTypes::kEvent;
      info.direction = BusDirections::kInput;
      info.channelCount = 1;
      setStr(info.name, u16 "Event in");
      info.busType = BusTypes::kMain;
      info.flags = BusInfo::kDefaultActive;

      return Steinberg::kResultTrue;
    }
    else
    {
      return Steinberg::kInvalidArgument;
    }
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    using namespace Steinberg::Vst;
    static const constexpr int output_event_busses
        = avnd::midi_output_introspection<T>::size;

    if (index < output_event_busses)
    {
      info.mediaType = MediaTypes::kEvent;
      info.direction = BusDirections::kOutput;
      info.channelCount = 1;
      setStr(info.name, u16 "Event Out");
      info.busType = BusTypes::kMain;
      info.flags = BusInfo::kDefaultActive;

      return Steinberg::kResultTrue;
    }
    else
    {
      return Steinberg::kInvalidArgument;
    }
  }

  Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < inputCount())
    {
      inputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < outputCount())
    {
      outputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  bool inputActive[std::max(inputCount(), 1)];
  bool outputActive[std::max(outputCount(), 1)];
};

static constexpr inline Steinberg::tresult
default_speaker_arrangement(int channels, Steinberg::Vst::SpeakerArrangement& arr)
{
  switch (channels)
  {
    case 0:
      arr = Steinberg::Vst::SpeakerArr::kEmpty;
      break;
    case 1:
      arr = Steinberg::Vst::SpeakerArr::kMono;
      break;
    case 2:
      arr = Steinberg::Vst::SpeakerArr::kStereo;
      break;
    case 3:
      arr = Steinberg::Vst::SpeakerArr::k30Music;
      break;
    case 4:
      arr = Steinberg::Vst::SpeakerArr::k40Music;
      break;
    case 5:
      arr = Steinberg::Vst::SpeakerArr::k50;
      break;
    case 6:
      arr = Steinberg::Vst::SpeakerArr::k60Music;
      break;
    case 7:
      arr = Steinberg::Vst::SpeakerArr::k70Music;
      break;
    case 8:
      arr = Steinberg::Vst::SpeakerArr::k80Music;
      break;
    case 9:
      arr = Steinberg::Vst::SpeakerArr::k90;
      break;
    case 10:
      arr = Steinberg::Vst::SpeakerArr::k100;
      break;
    case 11:
      arr = Steinberg::Vst::SpeakerArr::k110;
      break;
    case 12:
      return Steinberg::kResultFalse;
      break;
    case 13:
      arr = Steinberg::Vst::SpeakerArr::k130;
      break;
    case 14:
      arr = Steinberg::Vst::SpeakerArr::k140;
      break;
    default:
      return Steinberg::kResultFalse;
  }

  return Steinberg::kResultOk;
}

// Case where things are passed by argument to the "process" function,
// thus either the number of channels is set explicitely, or we default to 2 as
// that is the expectation for VSTs.

template <typename T>
struct audio_bus_info
{
  static constexpr int inputCount() noexcept { return 0; }
  static constexpr int outputCount() noexcept { return 0; }
  static constexpr int defaultInputChannelCount() noexcept { return 0; }
  static constexpr int defaultOutputChannelCount() noexcept { return 0; }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  inputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult setBusArrangements(
      avnd::effect_container<T>& effect,
      Steinberg::Vst::SpeakerArrangement* inputs,
      Steinberg::int32 numIns,
      Steinberg::Vst::SpeakerArrangement* outputs,
      Steinberg::int32 numOuts)
  {
    using namespace Steinberg;
    if (numIns == 0 && numOuts == 0)
      return kResultTrue;
    return kResultFalse;
  }

  static constexpr int runtime_input_channel_count = 0;
  static constexpr int runtime_output_channel_count = 0;
};

template <avnd::bus_arg_processor T>
struct audio_bus_info<T>
{
  static constexpr int inputCount() noexcept { return 1; }
  static constexpr int outputCount() noexcept { return 1; }
  static constexpr int defaultInputChannelCount() noexcept
  {
    return avnd::input_channels<T>(2);
  }
  static constexpr int defaultOutputChannelCount() noexcept
  {
    return avnd::output_channels<T>(2);
  }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    static const constexpr int input_audio_busses
        = avnd::bus_introspection<T>::input_busses;
    if (index == 0)
    {
      info.channelCount = avnd::input_channels<T>(2);
      setStr(info.name, u16 "Stereo In");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    static const constexpr int output_audio_busses
        = avnd::bus_introspection<T>::output_busses;

    if (index == 0)
    {
      info.channelCount = avnd::output_channels<T>(2);
      setStr(info.name, u16 "Stereo Out");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  inputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(avnd::input_channels<T>(2), arr);
  }

  static Steinberg::tresult
  outputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(avnd::output_channels<T>(2), arr);
  }

  Steinberg::tresult setBusArrangements(
      avnd::effect_container<T>& effect,
      Steinberg::Vst::SpeakerArrangement* inputs,
      Steinberg::int32 numIns,
      Steinberg::Vst::SpeakerArrangement* outputs,
      Steinberg::int32 numOuts)
  {
    using namespace stv3;
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (numIns == 1 && numOuts == 1)
    {
      runtime_input_channel_count = SpeakerArr::getChannelCount(inputs[0]);
      runtime_output_channel_count = SpeakerArr::getChannelCount(outputs[0]);

      effect.init_channels(runtime_input_channel_count, runtime_output_channel_count);

      return kResultTrue;
    }
    return kResultFalse;
  }

  Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index == 0)
    {
      inputActive = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index == 0)
    {
      outputActive = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  bool inputActive{};
  bool outputActive{};

  int runtime_input_channel_count = defaultInputChannelCount();
  int runtime_output_channel_count = defaultOutputChannelCount();
};

template <avnd::sample_arg_processor T>
struct audio_bus_info<T>
{
  static constexpr int inputCount() noexcept { return 1; }
  static constexpr int outputCount() noexcept { return 1; }
  static constexpr int defaultInputChannelCount() noexcept
  {
    return avnd::input_channels<T>(2);
  }
  static constexpr int defaultOutputChannelCount() noexcept
  {
    return avnd::output_channels<T>(2);
  }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index == 0)
    {
      info.channelCount = avnd::input_channels<T>(2);
      setStr(info.name, u16 "Mono In");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index == 0)
    {
      info.channelCount = avnd::output_channels<T>(2);
      setStr(info.name, u16 "Mono Out");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  inputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(avnd::input_channels<T>(2), arr);
  }

  static Steinberg::tresult
  outputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(avnd::output_channels<T>(2), arr);
  }

  Steinberg::tresult setBusArrangements(
      avnd::effect_container<T>& effect,
      Steinberg::Vst::SpeakerArrangement* inputs,
      Steinberg::int32 numIns,
      Steinberg::Vst::SpeakerArrangement* outputs,
      Steinberg::int32 numOuts)
  {
    using namespace stv3;
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (numIns == 1 && numOuts == 1)
    {
      runtime_input_channel_count = SpeakerArr::getChannelCount(inputs[0]);
      runtime_output_channel_count = SpeakerArr::getChannelCount(outputs[0]);

      effect.init_channels(runtime_input_channel_count, runtime_output_channel_count);

      return kResultTrue;
    }
    return kResultFalse;
  }

  Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index == 0)
    {
      inputActive = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index == 0)
    {
      outputActive = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  bool inputActive{};
  bool outputActive{};

  int runtime_input_channel_count = defaultInputChannelCount();
  int runtime_output_channel_count = defaultOutputChannelCount();
};

template <avnd::channel_arg_processor T>
struct audio_bus_info<T>
{
  static constexpr int inputCount() noexcept { return 1; }
  static constexpr int outputCount() noexcept { return 1; }
  static constexpr int defaultInputChannelCount() noexcept
  {
    return avnd::input_channels<T>(2);
  }
  static constexpr int defaultOutputChannelCount() noexcept
  {
    return avnd::output_channels<T>(2);
  }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index == 0)
    {
      info.channelCount = avnd::input_channels<T>(2);
      setStr(info.name, u16 "Mono In");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index == 0)
    {
      info.channelCount = avnd::output_channels<T>(2);
      setStr(info.name, u16 "Mono Out");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  inputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(avnd::input_channels<T>(2), arr);
  }

  static Steinberg::tresult
  outputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(avnd::output_channels<T>(2), arr);
  }

  Steinberg::tresult setBusArrangements(
      avnd::effect_container<T>& effect,
      Steinberg::Vst::SpeakerArrangement* inputs,
      Steinberg::int32 numIns,
      Steinberg::Vst::SpeakerArrangement* outputs,
      Steinberg::int32 numOuts)
  {
    using namespace stv3;
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (numIns == 1 && numOuts == 1)
    {
      runtime_input_channel_count = SpeakerArr::getChannelCount(inputs[0]);
      runtime_output_channel_count = SpeakerArr::getChannelCount(outputs[0]);

      effect.init_channels(runtime_input_channel_count, runtime_output_channel_count);

      return kResultTrue;
    }
    return kResultFalse;
  }

  Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index == 0)
    {
      inputActive = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index == 0)
    {
      outputActive = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  bool inputActive{};
  bool outputActive{};

  int runtime_input_channel_count = defaultInputChannelCount();
  int runtime_output_channel_count = defaultOutputChannelCount();
};

template <avnd::bus_port_processor T>
struct audio_bus_info<T>
{
  using input_refl = avnd::audio_bus_input_introspection<T>;
  using output_refl = avnd::audio_bus_output_introspection<T>;
  static constexpr int inputCount() noexcept { return input_refl::size; }
  static constexpr int outputCount() noexcept { return output_refl::size; }
  static constexpr int defaultInputChannelCount() noexcept
  {
    return avnd::input_channels<T>(2);
  }
  static constexpr int defaultOutputChannelCount() noexcept
  {
    return avnd::output_channels<T>(2);
  }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index < input_refl::size)
    {
      info.channelCount = 2;
      setStr(info.name, u16 "Stereo In");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index < output_refl::size)
    {
      info.channelCount = 2;
      setStr(info.name, u16 "Stereo Out");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  inputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    arr = Steinberg::Vst::SpeakerArr::kStereo;
    return Steinberg::kResultTrue;
  }

  static Steinberg::tresult
  outputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    arr = Steinberg::Vst::SpeakerArr::kStereo;
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult setBusArrangements(
      avnd::effect_container<T>& effect,
      Steinberg::Vst::SpeakerArrangement* inputs,
      Steinberg::int32 numIns,
      Steinberg::Vst::SpeakerArrangement* outputs,
      Steinberg::int32 numOuts)
  {
    using namespace stv3;
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (numIns == inputCount() && numOuts == outputCount())
    {
      // Count the total number of requested channels
      for (int i = 0; i < numIns; i++)
      {
        // TODO if bus channels are fixed, check that they are the same.
        // Otherwise..
        runtime_input_channel_count += SpeakerArr::getChannelCount(inputs[i]);
      }

      for (int i = 0; i < numOuts; i++)
      {
        runtime_output_channel_count += SpeakerArr::getChannelCount(outputs[i]);
      }

      effect.init_channels(runtime_input_channel_count, runtime_output_channel_count);

      return kResultTrue;
    }
    return kResultFalse;
  }

  Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < inputCount())
    {
      inputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < outputCount())
    {
      outputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  bool inputActive[std::max(inputCount(), 1)];
  bool outputActive[std::max(outputCount(), 1)];

  int runtime_input_channel_count = defaultInputChannelCount();
  int runtime_output_channel_count = defaultOutputChannelCount();
};

template <avnd::sample_port_processor T>
struct audio_bus_info<T>
{
  using input_refl = avnd::audio_bus_input_introspection<T>;
  using output_refl = avnd::audio_bus_output_introspection<T>;
  static constexpr int inputCount() noexcept { return input_refl::size; }
  static constexpr int outputCount() noexcept { return output_refl::size; }
  static constexpr int defaultInputChannelCount() noexcept { return inputCount(); }
  static constexpr int defaultOutputChannelCount() noexcept { return outputCount(); }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index < inputCount())
    {
      info.channelCount = 1;
      setStr(info.name, u16 "Main In");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index < outputCount())
    {
      info.channelCount = 1;
      setStr(info.name, u16 "Main Out");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  inputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(defaultInputChannelCount(), arr);
  }

  static Steinberg::tresult
  outputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(defaultOutputChannelCount(), arr);
  }

  Steinberg::tresult setBusArrangements(
      avnd::effect_container<T>& effect,
      Steinberg::Vst::SpeakerArrangement* inputs,
      Steinberg::int32 numIns,
      Steinberg::Vst::SpeakerArrangement* outputs,
      Steinberg::int32 numOuts)
  {
    using namespace stv3;
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (numIns == inputCount() && numOuts == outputCount())
    {
      // Count the total number of requested channels
      for (int i = 0; i < numIns; i++)
        if (SpeakerArr::getChannelCount(inputs[i]) != 1)
          return kResultFalse;
      for (int i = 0; i < numOuts; i++)
        if (SpeakerArr::getChannelCount(outputs[i]) != 1)
          return kResultFalse;

      return kResultTrue;
    }
    return kResultFalse;
  }

  Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < inputCount())
    {
      inputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < outputCount())
    {
      outputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  bool inputActive[std::max(inputCount(), 1)];
  bool outputActive[std::max(outputCount(), 1)];

  int runtime_input_channel_count = defaultInputChannelCount();
  int runtime_output_channel_count = defaultOutputChannelCount();
};

template <avnd::channel_port_processor T>
struct audio_bus_info<T>
{
  using input_refl = avnd::audio_bus_input_introspection<T>;
  using output_refl = avnd::audio_bus_output_introspection<T>;
  static constexpr int inputCount() noexcept { return input_refl::size; }
  static constexpr int outputCount() noexcept { return output_refl::size; }
  static constexpr int defaultInputChannelCount() noexcept { return inputCount(); }
  static constexpr int defaultOutputChannelCount() noexcept { return outputCount(); }

  static Steinberg::tresult
  inputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index < inputCount())
    {
      info.channelCount = 1;
      setStr(info.name, u16 "Main In");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  outputInfo(Steinberg::int32 index, Steinberg::Vst::BusInfo& info)
  {
    if (index < outputCount())
    {
      info.channelCount = 1;
      setStr(info.name, u16 "Main Out");
      info.busType = Steinberg::Vst::BusTypes::kMain;

      return Steinberg::kResultTrue;
    }

    return Steinberg::kInvalidArgument;
  }

  static Steinberg::tresult
  inputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(defaultInputChannelCount(), arr);
  }

  static Steinberg::tresult
  outputArrangement(Steinberg::int32 index, Steinberg::Vst::SpeakerArrangement& arr)
  {
    return default_speaker_arrangement(defaultOutputChannelCount(), arr);
  }

  Steinberg::tresult setBusArrangements(
      avnd::effect_container<T>& effect,
      Steinberg::Vst::SpeakerArrangement* inputs,
      Steinberg::int32 numIns,
      Steinberg::Vst::SpeakerArrangement* outputs,
      Steinberg::int32 numOuts)
  {
    using namespace stv3;
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (numIns == inputCount() && numOuts == outputCount())
    {
      // Count the total number of requested channels
      for (int i = 0; i < numIns; i++)
        if (SpeakerArr::getChannelCount(inputs[i]) != 1)
          return kResultFalse;
      for (int i = 0; i < numOuts; i++)
        if (SpeakerArr::getChannelCount(outputs[i]) != 1)
          return kResultFalse;

      return kResultTrue;
    }
    return kResultFalse;
  }

  Steinberg::tresult activateInput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < inputCount())
    {
      inputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  Steinberg::tresult activateOutput(Steinberg::int32 index, Steinberg::TBool state)
  {
    if (index < outputCount())
    {
      outputActive[index] = state;
      return Steinberg::kResultTrue;
    }
    return Steinberg::kInvalidArgument;
  }

  bool inputActive[std::max(inputCount(), 1)];
  bool outputActive[std::max(outputCount(), 1)];

  int runtime_input_channel_count = defaultInputChannelCount();
  int runtime_output_channel_count = defaultOutputChannelCount();
};

}
