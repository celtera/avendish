#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/common/export.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <CHOP_CPlusPlusBase.h>


namespace touchdesigner::chop
{
template <typename T>
struct audio_processor  : public TD::CHOP_CPlusPlusBase
{
  static constexpr int input_channels = avnd::input_channels<T>(0);
  static constexpr int output_channels = avnd::output_channels<T>(0);
  static constexpr int default_sample_rate = 44100;
  static constexpr int default_buffer_size = 4096;

  avnd::effect_container<T> implementation;
  avnd::process_adapter<T> processor;
  AVND_NO_UNIQUE_ADDRESS avnd::audio_channel_manager<T> channels;
  int runtime_input_count{input_channels};
  int runtime_output_count{output_channels};

  parameter_setup<T> param_setup;

  explicit audio_processor(const TD::OP_NodeInfo* info)
      : channels{this->implementation}
  {
    init();
  }

  void init()
  {
    avnd::init_controls(implementation);
    this->channels.set_input_channels(this->implementation, 0, 2);
    this->channels.set_output_channels(this->implementation, 0, 2);
  }

  void getGeneralInfo(TD::CHOP_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // No output: we're likely sending things over the network
    info->cookEveryFrame = avnd::bus_introspection<T>::output_busses == 0;
    info->cookEveryFrameIfAsked = true;
    info->inputMatchIndex = 0;
    info->timeslice = false;
  }

  int updateAudioChannels(const TD::OP_Inputs* inputs)
  {
    bool ok = true;
    int total_input_channels = 0;
    for(int i = 0;  i < inputs->getNumInputs(); i++) {
      auto td_in = inputs->getInputCHOP(i);

      int expected = td_in->numChannels;

      channels.set_input_channels(implementation, i, expected);
      int actual = channels.get_input_channels(implementation, i);
      total_input_channels += actual;
      ok &= (expected == actual);
    }
    return total_input_channels;
  }

  bool getOutputInfo(TD::CHOP_OutputInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    if constexpr(avnd::bus_introspection<T>::output_busses == 0)
    {
      info->numChannels = 0;
      info->numSamples = 0;
      info->startIndex = 0;
      info->sampleRate = 0;
      return true;
    }
    else if constexpr(avnd::bus_introspection<T>::output_busses == 1)
    {
      if(inputs->getNumInputs() == 0)
      {
        if(avnd::monophonic_audio_processor<T>) {
          return false;
        }

        // No input, use defaults
        info->numChannels = output_channels;
        info->numSamples = 1; // ??
        info->sampleRate = default_sample_rate; // ??
        info->startIndex = 0;

        // Allocate buffers that may be required for converting float <-> double
        avnd::process_setup setup_info{
                                       .input_channels = 0,
                                       .output_channels = output_channels,
                                       .frames_per_buffer = default_buffer_size,
                                       .rate = default_sample_rate};

        processor.allocate_buffers(setup_info, float{});

        implementation.init_channels(0, output_channels);

        // Allocate buffers if supported
        avnd::prepare(implementation, setup_info);


        return true;
      }

      const int total_input_channels = updateAudioChannels(inputs);
      const int output_channels = channels.get_output_channels(implementation, 0);
      auto input = inputs->getInputCHOP(0);
      assert(input);
      info->numChannels = output_channels;
      info->numSamples = input->numSamples;
      info->sampleRate = input->sampleRate;
      info->startIndex = input->startIndex;

      // Allocate buffers that may be required for converting float <-> double
      avnd::process_setup setup_info{
                                     .input_channels = total_input_channels,
                                     .output_channels = output_channels,
                                     .frames_per_buffer = default_buffer_size,
                                     .rate = input->sampleRate};
      processor.allocate_buffers(setup_info, float{});

      implementation.init_channels(input->numChannels, output_channels);

      // Allocate buffers if supported
      avnd::prepare(implementation, setup_info);


      return true;
    }
    else
    {
      return false;
      //static_assert(false, "Only one output bus is supported by TouchDesigner");
    }
  }

  void getChannelName(int32_t index, TD::OP_String* name, const TD::OP_Inputs* inputs, void* reserved) override
  {
    using busses = avnd::bus_introspection<T>;
    using channels = avnd::channels_introspection<T>;
    if(index < 0 || index >= channels::output_channels)
      return;

    if constexpr(requires { sizeof(busses::outputs); })
    {
      // FIXME right now we assume a single output bus.
      busses::outputs::for_nth_mapped(
          0,
          [&]<std::size_t Index, typename C>(avnd::field_reflection<Index, C> field) {
        if constexpr(avnd::has_name<C> || avnd::has_symbol<C> || avnd::has_c_name<C>)
        {
          static constexpr auto n = get_td_name<C>();
          name->setString(n.data());
        }
        else
        {
          name->setString(fmt::format("chan%1", index + 1).c_str());
        }
      });
    }
    else
    {
      name->setString(fmt::format("chan%1", index + 1).c_str());
    }
  }

  void execute(TD::CHOP_Output* output, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // FIXME TD is this weird hybrid shit where inputs are distinct busses but there's
    // only one output bus with all channels concatenated.

    // Update parameter values from TouchDesigner
    update_controls(inputs);

    // Get input CHOP
    if(inputs->getNumInputs() == 0)
    {
      if(avnd::monophonic_audio_processor<T>)
        return;

      // Prepare input/output spans for processing
      const int num_samples = output->numSamples;
      const int num_in_channels = 0;
      const int num_out_channels = output->numChannels;
      assert(num_out_channels == channels.get_output_channels(implementation, 0));

      float* in_ptrs[8]{0};
      auto out_ptrs = (float**) alloca(sizeof(float*) * num_out_channels + 4);

      for (int ch = 0; ch < num_out_channels; ++ch)
      {
        out_ptrs[ch] = output->channels[ch];
        assert(out_ptrs[ch]);
      }

      processor.process(implementation, std::span<float*>(in_ptrs, 0), std::span<float*>(out_ptrs, num_out_channels), num_samples);
      return;
    }
    else
    {
      const int total_input_channels = updateAudioChannels(inputs);
      const TD::OP_CHOPInput* input = inputs->getInputCHOP(0);
      assert(input);

      // Prepare input/output spans for processing
      const int num_samples = output->numSamples;
      const int num_in_channels = total_input_channels;
      const int num_out_channels = channels.get_output_channels(implementation, 0);

      auto in_ptrs = (float**) alloca(sizeof(float*) * num_in_channels + 4);
      auto out_ptrs = (float**) alloca(sizeof(float*) * num_out_channels + 4);

      for (int ch = 0; ch < num_in_channels; ++ch)
      {
        in_ptrs[ch] = (float*) input->getChannelData(ch);
        assert(in_ptrs[ch]);
      }

      for (int ch = 0; ch < num_out_channels; ++ch)
      {
        out_ptrs[ch] = output->channels[ch];
        assert(out_ptrs[ch]);
      }

      processor.process(implementation, std::span<float*>(in_ptrs, num_in_channels), std::span<float*>(out_ptrs, num_out_channels), num_samples);
    }
  }

  void setupParameters(TD::OP_ParameterManager* manager, void* reserved) override
  {
    param_setup.setup(implementation, manager);
  }

  int32_t
  getNumInfoCHOPChans(void *reserved1) override
  {
    return 0;
  }

  void
  getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved1) override
  {
  }

  bool
  getInfoDATSize(TD::OP_InfoDATSize* infoSize, void *reserved1) override
  {
    return false;
  }

  void
  getInfoDATEntries(int32_t index, int32_t nEntries,
                    TD:: OP_InfoDATEntries* entries,
                    void *reserved1) override
  {
  }

  void getWarningString(TD::OP_String *warning, void *reserved1)  override
  {
  }

  void
  getErrorString(TD::OP_String *error, void *reserved1)  override
  {
  }

  void
  getInfoPopupString(TD::OP_String *info, void *reserved1) override
  {
  }


  void
  pulsePressed(const char* name, void* reserved1) override
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.pulse(implementation, name);
  }

  void
  buildDynamicMenu(const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info, void* reserved1) override
  {
    // FIXME Used for enums
  }


private:
  // Helper to update control values from TD parameters
  void update_controls(const TD::OP_Inputs* inputs){
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.update(implementation, inputs);
  }
};
}
