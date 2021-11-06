#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <vst3/component_base.hpp>
#include <vst3/refcount.hpp>
#include <avnd/wrappers/([a-z_0-9]+)\.hpp>
#include <avnd/wrappers/([a-z_0-9]+)\.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/([a-z_0-9]+)\.hpp>

namespace stv3
{
template<typename T>
struct Component final
    : public stv3::ComponentCommon
    , public Steinberg::Vst::IAudioProcessor
    , public Steinberg::Vst::IProcessContextRequirements
    , stv3::refcount
{
  using BusInfo = Steinberg::Vst::BusInfo;
  using MediaType = Steinberg::Vst::MediaType;
  using MediaTypes = Steinberg::Vst::MediaTypes;
  using BusDirections = Steinberg::Vst::BusDirections;
  using BusDirection = Steinberg::Vst::BusDirection;
  using BusType = Steinberg::Vst::BusType;
  using BusTypes = Steinberg::Vst::BusTypes;
  using Event = Steinberg::Vst::Event;
  using SpeakerArrangement = Steinberg::Vst::SpeakerArrangement;
  using ProcessData = Steinberg::Vst::ProcessData;
  using ProcessSetup = Steinberg::Vst::ProcessSetup;
  using IParameterChanges = Steinberg::Vst::IParameterChanges;
  using IParamValueQueue = Steinberg::Vst::IParamValueQueue;
  using IEventList = Steinberg::Vst::IEventList;
  using IStreamAttributes = Steinberg::Vst::IStreamAttributes;
  using IAttributeList = Steinberg::Vst::IAttributeList;
  using String128 = Steinberg::Vst::String128;
  using IBStream = Steinberg::IBStream;
  using IBStreamer = Steinberg::IBStreamer;
  using ParamValue = Steinberg::Vst::ParamValue;
  using ParamID = Steinberg::Vst::ParamID;
  using CtrlNumber = Steinberg::Vst::CtrlNumber;

  // FUnknown stuff:
  Steinberg::tresult
  queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, IAudioProcessor::iid, IAudioProcessor);
    QUERY_INTERFACE(iid, obj, IProcessContextRequirements::iid, IProcessContextRequirements);
    QUERY_INTERFACE(iid, obj, IComponent::iid, IComponent);
    QUERY_INTERFACE(iid, obj, IPluginBase::iid, IPluginBase);
    QUERY_INTERFACE(iid, obj, IConnectionPoint::iid, IConnectionPoint);
    QUERY_INTERFACE(iid, obj, IDependent::iid, IDependent);
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }

  uint32 addRef() override { return ++m_refcount; }
  uint32 release() override {
    if (--m_refcount == 0)
    {
      delete this;
      return 0;
    }
    return m_refcount;
  }

  avnd::effect_container<T> effect;

  [[no_unique_address]]
  avnd::process_adapter<T> processor;

  [[no_unique_address]]
  avnd::midi_storage<T> midi;

  static const constexpr int input_audio_busses = avnd::bus_introspection<T>::input_busses;
  static const constexpr int output_audio_busses = avnd::bus_introspection<T>::output_busses;
  static const constexpr int input_event_busses = avnd::midi_input_introspection<T>::size;
  static const constexpr int output_event_busses = avnd::midi_output_introspection<T>::size;

  static const constexpr int input_channels = avnd::input_channels<T>(2);
  static const constexpr int output_channels = avnd::output_channels<T>(2);

  int runtime_input_channel_count = input_channels;
  int runtime_output_channel_count = output_channels;

  using inputs_info_t = avnd::parameter_input_introspection<T>;
  static const constexpr int32_t parameter_count = inputs_info_t::size;

  bool audioInputActive[input_audio_busses];
  bool audioOutputActive[output_audio_busses];
  bool eventInputActive[input_event_busses];
  bool eventOutputActive[output_event_busses];

  static constexpr AudioBusInfo audioInputs[]{
    {
      /* .mediaType = */ MediaTypes::kAudio,
          /* .direction = */ BusDirections::kInput,
          /* .channelCount = */ 2,
          /* .name = */ u"Stereo In",
          /* .busType = */ BusTypes::kMain,
          /* .flags = */ BusInfo::kDefaultActive,
          /* .arrangement = */ Steinberg::Vst::SpeakerArr::kStereo}
  };

  static constexpr AudioBusInfo audioOutputs[]
  {
    {
      /* .mediaType = */ MediaTypes::kAudio,
          /* .direction = */ BusDirections::kOutput,
          /* .channelCount = */ 2,
          /* .name = */ u"Stereo Out",
          /* .busType = */ BusTypes::kMain,
          /* .flags = */ BusInfo::kDefaultActive,
          /* .arrangement = */ Steinberg::Vst::SpeakerArr::kStereo}
  };

  static constexpr EventBusInfo eventInputs[]{
//    {
//      /* .mediaType = */ MediaTypes::kEvent,
//          /* .direction = */ BusDirections::kInput,
//          /* .channelCount = */ 1,
//          /* .name = */ u"Event In",
//          /* .busType = */ BusTypes::kMain,
//          /* .flags = */ BusInfo::kDefaultActive,
//    }
  };

  static constexpr EventBusInfo eventOutputs[]{

  };

  Component()
  {
    using namespace Steinberg::Vst;

    currentProcessMode = -1;
    processSetup.maxSamplesPerBlock = 1024;
    processSetup.processMode = kRealtime;
    processSetup.sampleRate = 44100.0;
    processSetup.symbolicSampleSize = kSample32;

    // Setup modules
    effect.init_channels(input_channels, output_channels);

    /// Read the initial state of the controls

    // First the default value
    avnd::init_controls(effect.inputs());
  }

  virtual ~Component()
  {

  }

  Steinberg::tresult getControllerClassId(Steinberg::TUID classID) override
  {
    static constexpr auto tuid = stv3::controller_uuid_for_type<T>();
    memcpy(classID, &tuid, sizeof(Steinberg::TUID));
    return Steinberg::kResultTrue;
  }




  /****************/
  /**** BUSSES ****/
  /****************/

  int32 getBusCount(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection dir) override
  {
    using namespace Steinberg::Vst;
    switch (type)
    {
      case kAudio:
      {
        switch (dir)
        {
          case kInput:
            return input_audio_busses;
          case kOutput:
            return output_audio_busses;
          default:
            break;
        }
      }
      case kEvent:
      {
        switch (dir)
        {
          case kInput:
            return input_event_busses;
          case kOutput:
            return output_event_busses;
          default:
            break;
        }
      }
    }
    return 0;
  }

  Steinberg::tresult
  activateBus(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection dir, int32 index, Steinberg::TBool state)
      override
  {
    using namespace Steinberg::Vst;
    if (index < 0)
      return Steinberg::kInvalidArgument;

    switch (type)
    {
      case kAudio:
      {
        switch (dir)
        {
          case kInput:
            if(index < input_audio_busses) {
              audioInputActive[index] = state;
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          case kOutput:
            if(index < output_audio_busses) {
              audioOutputActive[index] = state;
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          default:
            break;
        }
      }
      case kEvent:
      {
        switch (dir)
        {
          case kInput:
            if(index < input_event_busses) {
              eventInputActive[index] = state;
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          case kOutput:
            if(index < output_event_busses) {
              eventOutputActive[index] = state;
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          default:
            break;
        }
      }
    }

    return Steinberg::kInvalidArgument;
  }

  tresult getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& info) override
  {
    using namespace Steinberg::Vst;
    if (index < 0)
      return Steinberg::kInvalidArgument;

    switch (type)
    {
      case kAudio:
      {
        switch (dir)
        {
          case kInput:
            if(index < input_audio_busses) {
              memcpy(&info, &audioInputs[index], sizeof(BusInfo));
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          case kOutput:
            if(index < output_audio_busses) {
              memcpy(&info, &audioOutputs[index], sizeof(BusInfo));
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          default:
            break;
        }
      }
      case kEvent:
      {
        switch (dir)
        {
          case kInput:
            if(index < input_event_busses) {
              memcpy(&info, &eventInputs[index], sizeof(BusInfo));
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          case kOutput:
            if(index < output_event_busses) {
              memcpy(&info, &eventOutputs[index], sizeof(BusInfo));
              return Steinberg::kResultTrue;
            } else {
              return Steinberg::kInvalidArgument;
            }
          default:
            break;
        }
      }
    }

    return Steinberg::kInvalidArgument;
  }

  tresult renameBus(
      MediaType type,
      BusDirection dir,
      int32 index,
      const String128 newName)
  {
    return Steinberg::kResultFalse;
  }

  tresult getBusArrangement(
      BusDirection dir,
      int32 index,
      SpeakerArrangement& arr) override
  {
    using namespace Steinberg::Vst;
    if (index < 0)
      return Steinberg::kInvalidArgument;

    switch (dir)
    {
      case kInput:
        if(index < input_audio_busses) {
          arr = audioInputs[index].arrangement;
          return Steinberg::kResultTrue;
        } else {
          return Steinberg::kInvalidArgument;
        }
      case kOutput:
        if(index < output_audio_busses) {
          arr = audioOutputs[index].arrangement;
          return Steinberg::kResultTrue;
        } else {
          return Steinberg::kInvalidArgument;
        }
      default:
        break;
    }
    return Steinberg::kInvalidArgument;
  }

  tresult setBusArrangements(
      SpeakerArrangement* inputs,
      int32 numIns,
      SpeakerArrangement* outputs,
      int32 numOuts) override
  {
    using namespace stv3;
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if(numIns == input_audio_busses && numOuts == output_audio_busses)
    {
      // Count the total number of requested channels
      for(int i = 0; i < numIns; i++)
        runtime_input_channel_count += SpeakerArr::getChannelCount(inputs[i]);

      for(int i = 0; i < numOuts; i++)
        runtime_output_channel_count += SpeakerArr::getChannelCount(outputs[i]);

      effect.init_channels(runtime_input_channel_count, runtime_output_channel_count);

      // TODO we may have to recopy the controls
      return kResultTrue;
    }
    return kResultFalse;
  }






  /*****************/
  /**** PROCESS ****/
  /*****************/

  uint32 getProcessContextRequirements() override
  {
    // TODO tempo etc
    return {};
  }

  tresult setupProcessing(ProcessSetup& newSetup) override
  {
    using namespace Steinberg;
    // called before the process call, always in a disable state (not active)

    // here we keep a trace of the processing mode (offline,...) for example.
    currentProcessMode = newSetup.processMode;

    processSetup.maxSamplesPerBlock = newSetup.maxSamplesPerBlock;
    processSetup.processMode = newSetup.processMode;
    processSetup.sampleRate = newSetup.sampleRate;
    processSetup.symbolicSampleSize = newSetup.symbolicSampleSize;

    /// TODO ///
    // this part can be refactored with vintage

    // Allocate buffers, setup everything
    avnd::process_setup setup_info{
        .input_channels = runtime_input_channel_count,
        .output_channels = runtime_output_channel_count,
        .frames_per_buffer = newSetup.maxSamplesPerBlock,
        .rate = newSetup.sampleRate};

    // Setup buffers for eventual float <-> double conversion
    // We always do so for float, as the plug-in API requires the existence of this
    processor.allocate_buffers(setup_info, float{});
    processor.allocate_buffers(setup_info, double{});

    // Setup buffers for storing MIDI messages
    if constexpr (avnd::midi_input_introspection<T>::size > 0)
    {
      using i_info = avnd::midi_input_introspection<T>;
      auto& in_port = boost::pfr::get<i_info::index_map[0]>(effect.inputs());

      midi.reserve_space(in_port, newSetup.maxSamplesPerBlock);
    }

    // Effect-specific preparation
    avnd::prepare(effect, setup_info);
    return kResultOk;
  }

  void processControl(IParamValueQueue& queue)
  {
    ParamValue value;
    int32 sampleOffset;
    int32 numPoints = queue.getPointCount();

    int id = queue.getParameterId();
    if (queue.getPoint(numPoints - 1, sampleOffset, value) == Steinberg::kResultTrue)
    {
      avnd::parameter_input_introspection<T>::for_nth(
            effect.inputs(), id,
            [&] <typename C> (C& ctl) {
        ctl.value = avnd::map_control_from_01<C>(value);
      });
    };
  }

  void processControls(ProcessData& data)
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (auto paramChanges = data.inputParameterChanges)
    {
      int32 numParamsChanged = paramChanges->getParameterCount();
      // for each parameter which are some changes in this audio block:
      for (int32 i = 0; i < numParamsChanged; i++)
      {
        if (auto q = paramChanges->getParameterData(i))
        {
          processControl(*q);
        }
      }
    }
  }


  void add_message(avnd::raw_container_midi_port auto& bus, uint8_t a, uint8_t b, uint8_t c, auto ts)
  {
    // TODO
  }
  void add_message(avnd::dynamic_container_midi_port auto& bus, uint8_t a, uint8_t b, uint8_t c, auto ts)
  {
    bus.midi_messages.push_back({
                                  .bytes = {a,b,c},
                                  .timestamp = ts
                                });
  }


  void processEvent(avnd::midi_port auto& bus, const Steinberg::Vst::NoteOnEvent& ev, auto ts)
  {
    add_message(bus, ev.channel | 0x90, ev.pitch, ev.velocity * 127, ts);
  }

  void processEvent(avnd::midi_port auto& bus, const Steinberg::Vst::NoteOffEvent& ev, auto ts)
  {
    add_message(bus, ev.channel | 0x80, ev.pitch, ev.velocity * 127, ts);
  }

  void processEvent(Event& event)
  {
    using refl = avnd::midi_input_introspection<T>;
    // 0 <= event.busIndex < count(midi_ports)
    // thus we have to map to the actual port index in our input struct
    const int idx = refl::index_map[event.busIndex];
    switch (event.type)
    {
      case Event::kNoteOnEvent:
      {
        refl::for_nth(this->effect.inputs(), idx, [&] (auto& bus) { this->processEvent(bus, event.noteOn, event.sampleOffset); });
        break;
      }
      case Event::kNoteOffEvent:
      {
        refl::for_nth(this->effect.inputs(), idx, [&] (auto& bus) { this->processEvent(bus, event.noteOff, event.sampleOffset); });
        break;
      }
      case Event::kDataEvent:
      {
        auto& e = event.data;
        break;
      }
      case Event::kPolyPressureEvent:
      {
        auto& e = event.polyPressure;
        break;
      }
      case Event::kNoteExpressionValueEvent:
      {
        auto& e = event.noteExpressionValue;
        break;
      }
      case Event::kNoteExpressionTextEvent:
      {
        auto& e = event.noteExpressionText;
        break;
      }
      case Event::kChordEvent:
      {
        auto& e = event.chord;
        break;
      }
      case Event::kScaleEvent:
      {
        auto& e = event.scale;
        break;
      }
      case Event::kLegacyMIDICCOutEvent:
      {
        auto& e = event.midiCCOut;
        break;
      }
    }

  }

  void processEvents(ProcessData& data)
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (data.inputEvents)
    {
      const int32 numEvent = data.inputEvents->getEventCount();
      for (int32 i = 0; i < numEvent; i++)
      {
        Event event;
        if (data.inputEvents->getEvent(i, event) == kResultOk)
        {
          processEvent(event);
        }
      }
    }
  }

  void processAudio(ProcessData& data)
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    // FIXME handle multiple busses !

    auto in = stv3::getChannelBuffersPointer(processSetup, data.inputs[0]);
    auto out = stv3::getChannelBuffersPointer(processSetup, data.outputs[0]);

    data.outputs[0].silenceFlags = 0;
    if (data.symbolicSampleSize == kSample32)
    {
      processor.process(
            effect,
            std::span{(Sample32**)in, std::size_t(data.inputs[0].numChannels)},
            std::span{(Sample32**)out, std::size_t(data.outputs[0].numChannels)},
            data.numSamples);
    }
    else
    {
      processor.process(
            effect,
            std::span{(Sample64**)in, std::size_t(data.inputs[0].numChannels)},
            std::span{(Sample64**)out, std::size_t(data.outputs[0].numChannels)},
            data.numSamples);
    }
  }


  void processOutputs(ProcessData& data)
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    // TODO
  }

  tresult process(ProcessData& data) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    processControls(data);
    processEvents(data);

    if (data.numInputs != 0 && data.numOutputs != 0)
    {
      processAudio(data);
      processOutputs(data);
    }

    // Clear our midi ports
    if constexpr (avnd::midi_input_introspection<T>::size > 0)
    {
      using i_info = avnd::midi_input_introspection<T>;
      i_info::for_all(effect.inputs, [&] (auto& midi_port) {
        this->midi.clear(midi_port);
      });
    }
    return kResultOk;
  }






  /****************/
  /**** SAVING ****/
  /****************/

  tresult setState(IBStream* state) override
  {
    using namespace Steinberg;
    // called when we load a preset, the model has to be reloaded

    IBStreamer streamer(state, kLittleEndian);
    bool ok = inputs_info_t::for_all_unless(
          this->effect.inputs(),
          [&] <typename C> (C& field) -> bool{
            double param = 0.f;
            if (streamer.readDouble(param) == false)
              return false;
            field.value = avnd::map_control_from_01<C>(param);
            return true;
          });

    return ok ? Steinberg::kResultOk : Steinberg::kResultFalse;
  }

  tresult getState(IBStream* state) override
  {
    using namespace Steinberg;

    IBStreamer streamer(state, kLittleEndian);

    bool ok = inputs_info_t::for_all_unless(
          this->effect.inputs(),
          [&] <typename C> (C& field) -> bool{
            double param = avnd::map_control_to_01<C>(field.value);
            if (streamer.writeDouble(param) == false)
              return false;
            avnd::map_control_from_01(field, param);
            return true;
          });

    return ok ? Steinberg::kResultOk : Steinberg::kResultFalse;
  }


  /****************/
  /**** CONFIG ****/
  /****************/
  tresult canProcessSampleSize(int32 symbolicSampleSize) override
  {
    // We'll do conversions if necessary, 32 & 64 sample sizes are supported
    return Steinberg::kResultTrue;
  }

  uint32 getLatencySamples() override
  {
    return 0;
  }

  tresult setProcessing(TBool state) override
  {
    using namespace Steinberg;
    return kNotImplemented;
  }

  uint32 getTailSamples() override
  {
    using namespace Steinberg::Vst;
    return kNoTail;
  }

  Steinberg::tresult setActive(Steinberg::TBool state) override
  {
    using namespace Steinberg;
    return kResultOk;
  }

  ProcessSetup processSetup;

  int32 currentProcessMode{};
};

}
