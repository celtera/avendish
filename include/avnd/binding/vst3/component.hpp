#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vst3/bus_info.hpp>
#include <avnd/binding/vst3/component_base.hpp>
#include <avnd/binding/vst3/helpers.hpp>
#include <avnd/binding/vst3/refcount.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/midi.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>

namespace stv3
{

template <typename T>
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
  Steinberg::tresult queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, IAudioProcessor::iid, IAudioProcessor);
    QUERY_INTERFACE(
        iid, obj, IProcessContextRequirements::iid, IProcessContextRequirements);
    QUERY_INTERFACE(iid, obj, IComponent::iid, IComponent);
    QUERY_INTERFACE(iid, obj, IPluginBase::iid, IPluginBase);
    QUERY_INTERFACE(iid, obj, IConnectionPoint::iid, IConnectionPoint);
    QUERY_INTERFACE(iid, obj, IDependent::iid, IDependent);
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }

  uint32 addRef() override { return ++m_refcount; }
  uint32 release() override
  {
    if (--m_refcount == 0)
    {
      delete this;
      return 0;
    }
    return m_refcount;
  }

  avnd::effect_container<T> effect;

  [[no_unique_address]] avnd::process_adapter<T> processor;

  [[no_unique_address]] avnd::midi_storage<T> midi;

  [[no_unique_address]] stv3::audio_bus_info<T> audio_busses;

  [[no_unique_address]] stv3::event_bus_info<T> event_busses;

  using inputs_info_t = avnd::parameter_input_introspection<T>;
  static const constexpr int32_t parameter_count = inputs_info_t::size;

  Component()
  {
    using namespace Steinberg::Vst;

    currentProcessMode = -1;
    processSetup.maxSamplesPerBlock = 1024;
    processSetup.processMode = kRealtime;
    processSetup.sampleRate = 44100.0;
    processSetup.symbolicSampleSize = kSample32;

    // Setup modules
    effect.init_channels(
        audio_busses.defaultInputChannelCount(),
        audio_busses.defaultOutputChannelCount());

    /// Read the initial state of the controls

    // First the default value
    avnd::init_controls(effect);
  }

  virtual ~Component() { }

  Steinberg::tresult getControllerClassId(Steinberg::TUID classID) override
  {
    static constexpr auto tuid = stv3::controller_uuid_for_type<T>();
    memcpy(classID, &tuid, sizeof(Steinberg::TUID));
    return Steinberg::kResultTrue;
  }

  /****************/
  /**** BUSSES ****/
  /****************/

  int32
  getBusCount(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection dir) override
  {
    using namespace Steinberg::Vst;
    switch (type)
    {
      case kAudio:
      {
        switch (dir)
        {
          case kInput:
            return audio_busses.inputCount();
          case kOutput:
            return audio_busses.outputCount();
          default:
            break;
        }
      }
      case kEvent:
      {
        switch (dir)
        {
          case kInput:
            return event_busses.inputCount();
          case kOutput:
            return event_busses.outputCount();
          default:
            break;
        }
      }
    }
    return 0;
  }

  Steinberg::tresult activateBus(
      Steinberg::Vst::MediaType type,
      Steinberg::Vst::BusDirection dir,
      int32 index,
      Steinberg::TBool state) override
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
            return audio_busses.activateInput(index, state);
          case kOutput:
            return audio_busses.activateOutput(index, state);
          default:
            break;
        }
      }
      case kEvent:
      {
        switch (dir)
        {
          case kInput:
            return event_busses.activateInput(index, state);
          case kOutput:
            return event_busses.activateOutput(index, state);
          default:
            break;
        }
      }
    }

    return Steinberg::kInvalidArgument;
  }

  tresult
  getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& info) override
  {
    using namespace Steinberg::Vst;
    if (index < 0)
      return Steinberg::kInvalidArgument;

    switch (type)
    {
      case kAudio:
      {
        info.mediaType = MediaTypes::kAudio;
        info.flags = BusInfo::kDefaultActive;
        switch (dir)
        {
          case kInput:
            info.direction = BusDirections::kInput;
            return audio_busses.inputInfo(index, info);
          case kOutput:
            info.direction = BusDirections::kOutput;
            return audio_busses.outputInfo(index, info);
          default:
            break;
        }
      }
      case kEvent:
      {
        switch (dir)
        {
          case kInput:
            info.direction = BusDirections::kInput;
            return event_busses.inputInfo(index, info);
          case kOutput:
            info.direction = BusDirections::kOutput;
            return event_busses.outputInfo(index, info);
          default:
            break;
        }
      }
    }

    return Steinberg::kInvalidArgument;
  }

  tresult
  renameBus(MediaType type, BusDirection dir, int32 index, const String128 newName)
  {
    return Steinberg::kResultFalse;
  }

  tresult
  getBusArrangement(BusDirection dir, int32 index, SpeakerArrangement& arr) override
  {
    using namespace Steinberg::Vst;
    if (index < 0)
      return Steinberg::kInvalidArgument;

    switch (dir)
    {
      case kInput:
        return audio_busses.inputArrangement(index, arr);
      case kOutput:
        return audio_busses.outputArrangement(index, arr);
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

    return audio_busses.setBusArrangements(effect, inputs, numIns, outputs, numOuts);
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
        .input_channels = audio_busses.runtime_input_channel_count,
        .output_channels = audio_busses.runtime_output_channel_count,
        .frames_per_buffer = newSetup.maxSamplesPerBlock,
        .rate = newSetup.sampleRate};

    // Setup buffers for eventual float <-> double conversion
    // We always do so for float, as the plug-in API requires the existence of this
    processor.allocate_buffers(setup_info, float{});
    processor.allocate_buffers(setup_info, double{});

    effect.init_channels(
        audio_busses.runtime_input_channel_count,
        audio_busses.runtime_output_channel_count);

    // Setup buffers for storing MIDI messages
    if constexpr (avnd::midi_input_introspection<T>::size > 0)
    {
      using i_info = avnd::midi_input_introspection<T>;
      auto& in_port = avnd::pfr::get<i_info::index_map[0]>(effect.inputs());

      midi.reserve_space(this->effect, newSetup.maxSamplesPerBlock);
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
      avnd::parameter_input_introspection<T>::for_nth_raw(
          effect.inputs(),
          id,
          [&]<typename C>(C& ctl) { ctl.value = avnd::map_control_from_01<C>(value); });
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

  template <typename Bus>
  void add_message(Bus& bus, uint8_t a, uint8_t b, uint8_t c, auto ts)
  {
    if constexpr (avnd::dynamic_container_midi_port<Bus>)
    {
      bus.midi_messages.push_back({.bytes = {a, b, c}, .timestamp = ts});
    }
    else
    {
      //AVND_STATIC_TODO(Bus);
    }
  }

  void
  processEvent(avnd::midi_port auto& bus, const Steinberg::Vst::NoteOnEvent& ev, auto ts)
  {
    add_message(bus, ev.channel | 0x90, ev.pitch, ev.velocity * 127, ts);
  }

  void processEvent(
      avnd::midi_port auto& bus,
      const Steinberg::Vst::NoteOffEvent& ev,
      auto ts)
  {
    add_message(bus, ev.channel | 0x80, ev.pitch, ev.velocity * 127, ts);
  }

  void processEvent(Event& event)
  {
    using refl = avnd::midi_input_introspection<T>;
    // 0 <= event.busIndex < count(midi_ports)
    // thus we have to map to the actual port index in our input struct
    switch (event.type)
    {
      case Event::kNoteOnEvent:
      {
        refl::for_nth_mapped(
            this->effect.inputs(),
            event.busIndex,
            [&](auto& bus)
            { this->processEvent(bus, event.noteOn, event.sampleOffset); });
        break;
      }
      case Event::kNoteOffEvent:
      {
        refl::for_nth_mapped(
            this->effect.inputs(),
            event.busIndex,
            [&](auto& bus)
            { this->processEvent(bus, event.noteOff, event.sampleOffset); });
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
          avnd::span<Sample32*>{(Sample32**)in, std::size_t(data.inputs[0].numChannels)},
          avnd::span<Sample32*>{
              (Sample32**)out, std::size_t(data.outputs[0].numChannels)},
          data.numSamples);
    }
    else
    {
      processor.process(
          effect,
          avnd::span<Sample64*>{(Sample64**)in, std::size_t(data.inputs[0].numChannels)},
          avnd::span<Sample64*>{
              (Sample64**)out, std::size_t(data.outputs[0].numChannels)},
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

    // Clear outputs
    this->midi.clear_outputs(effect);

    processControls(data);
    processEvents(data);

    if (data.numInputs != 0 && data.numOutputs != 0)
    {
      processAudio(data);
      processOutputs(data);
    }

    // Clear inputs
    this->midi.clear_inputs(effect);

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
    if constexpr (avnd::has_inputs<T>)
    {
      bool ok = inputs_info_t::for_all_unless(
          this->effect.inputs(),
          [&]<typename C>(C& field) -> bool
          {
            double param = 0.f;
            if (streamer.readDouble(param) == false)
              return false;
            field.value = avnd::map_control_from_01<C>(param);
            return true;
          });

      return ok ? Steinberg::kResultOk : Steinberg::kResultFalse;
    }
    else
    {
      return Steinberg::kResultOk;
    }
  }

  tresult getState(IBStream* state) override
  {
    using namespace Steinberg;

    IBStreamer streamer(state, kLittleEndian);
    if constexpr (avnd::has_inputs<T>)
    {
      bool ok = inputs_info_t::for_all_unless(
          this->effect.inputs(),
          [&]<typename C>(C& field) -> bool
          {
            double param = avnd::map_control_to_01<C>(field.value);
            return streamer.writeDouble(param);
          });

      return ok ? Steinberg::kResultOk : Steinberg::kResultFalse;
    }
    else
    {
      return Steinberg::kResultOk;
    }
  }

  /****************/
  /**** CONFIG ****/
  /****************/
  tresult canProcessSampleSize(int32 symbolicSampleSize) override
  {
    // We'll do conversions if necessary, 32 & 64 sample sizes are supported
    return Steinberg::kResultTrue;
  }

  uint32 getLatencySamples() override { return 0; }

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
