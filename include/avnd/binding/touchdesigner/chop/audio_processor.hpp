#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/common/export.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <CHOP_CPlusPlusBase.h>
#include <cstring>
#include <span>
#include <string>
#include <cstring>
#include <vector>


/**
 * TouchDesigner CHOP audio processor binding
 *
 * This file implements the wrapper for Avendish audio processors as TouchDesigner CHOPs
 * Following the pattern established in Max/PD bindings
 */

// Forward declare TD types (actual headers will be included in implementation)
namespace TD {
class CHOP_GeneralInfo;
class CHOP_OutputInfo;
class CHOP_Output;
class OP_Inputs;
class OP_ParameterManager;
class OP_String;
class OP_CHOPInput;
struct OP_NodeInfo;
}

namespace touchdesigner::chop
{
template <typename T>
struct audio_processor  : public TD::CHOP_CPlusPlusBase
{
  // Channel configuration from Avendish introspection
  static constexpr int input_channels = avnd::input_channels<T>(1);
  static constexpr int output_channels = avnd::output_channels<T>(1);

  // Avendish components
  avnd::effect_container<T> implementation;
  avnd::process_adapter<T> processor;

  // TouchDesigner parameter handling
  parameter_setup<T> param_setup;
  using inputs_info_t = avnd::parameter_input_introspection<T>;
  static const constexpr int32_t parameter_count = inputs_info_t::size;

  // Runtime channel counts (can differ from static if dynamic)
  int runtime_input_count{input_channels};
  int runtime_output_count{output_channels};

  // Initialization
  explicit audio_processor(const TD::OP_NodeInfo* info)
  {
    init();
  }

  void init()
  {
    // Initialize controls with default values
    avnd::init_controls(implementation);

    // Initialize polyphony if supported
    if constexpr (avnd::can_prepare<T>)
    {
      implementation.init_channels(input_channels, output_channels);
    }
  }

  // TouchDesigner interface methods (will be pure virtual when inheriting from base)
  void getGeneralInfo(TD::CHOP_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Set cooking behavior
    info->cookEveryFrameIfAsked = false;

    // Match input 0 by default for channel/sample configuration
    info->inputMatchIndex = 0;

    // We're not using timeslice mode for now
    info->timeslice = false;
  }

  bool getOutputInfo(TD::CHOP_OutputInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    if(inputs->getNumInputs() == 0)
    {
      // No input, use defaults
      info->numChannels = output_channels;
      info->numSamples = 1; // ??
      info->sampleRate = 44100.0f; // ??
      info->startIndex = 0;
      {
        // Allocate buffers that may be required for converting float <-> double
        avnd::process_setup setup_info{
                                       .input_channels = 0,
                                       .output_channels = output_channels,
                                       .frames_per_buffer = 4096,
                                       .rate = 44100};
        processor.allocate_buffers(setup_info, float{});

        // Allocate buffers if supported
        avnd::prepare(implementation, setup_info);
      }
      return true;
    }

    // Get input CHOP if available
    const TD::OP_CHOPInput* input = inputs->getInputCHOP(0);

    if (input)
    {
      // Match input configuration
      info->numChannels = input->numChannels;
      info->numSamples = input->numSamples;
      info->sampleRate = static_cast<float>(input->sampleRate);
      info->startIndex = static_cast<uint32_t>(input->startIndex);
      {
        // Allocate buffers that may be required for converting float <-> double
        avnd::process_setup setup_info{
                                       .input_channels = input->numChannels,
                                       .output_channels = output_channels,
                                       .frames_per_buffer = 4096,
                                       .rate = 44100};
        processor.allocate_buffers(setup_info, float{});

        // Allocate buffers if supported
        avnd::prepare(implementation, setup_info);
      }

    }
    else
    {
      // No input, use defaults
      info->numChannels = output_channels;
      info->numSamples = 1;
      info->sampleRate = 44100.0f;
      info->startIndex = 0;
      {
        // Allocate buffers that may be required for converting float <-> double
        avnd::process_setup setup_info{
                                       .input_channels = 0,
                                       .output_channels = output_channels,
                                       .frames_per_buffer = 4096,
                                       .rate = 44100};
        processor.allocate_buffers(setup_info, float{});

        // Allocate buffers if supported
        avnd::prepare(implementation, setup_info);
      }
    }

    return true; // We specify output info
  }

  void getChannelName(int32_t index, TD::OP_String* name, const TD::OP_Inputs* inputs, void* reserved) override
  {
    if(index < 0 || index >= inputs_info_t::size)
      return;

    using busses = avnd::bus_introspection<T>;
    if constexpr(requires { sizeof(busses::outputs); })
    {
      busses::outputs::for_nth_mapped(
          index,
          [&]<std::size_t Index, typename C>(avnd::field_reflection<Index, C> field) {
        if constexpr(avnd::has_name<C>)
        {
          name->setString(avnd::get_name<C>().data());
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

  void execute(TD::CHOP_Output* output, const TD::OP_Inputs* inputs, void* reserved) override{
    // Update parameter values from TouchDesigner
    update_controls(inputs);

    // Get input CHOP
    if(inputs->getNumInputs() == 0)
      return;

    const TD::OP_CHOPInput* input = inputs->getInputCHOP(0);

    if (!input)
    {
      // No input, output silence
      for (int i = 0; i < output->numChannels; ++i)
      {
        std::memset(output->channels[i], 0, output->numSamples * sizeof(float));
      }
      return;
    }

    // Prepare input/output spans for processing
    // TD uses float, convert to double if needed
    const int num_samples = output->numSamples;
    const int num_in_channels = input->numChannels;
    const int num_out_channels = output->numChannels;

    // Check if processor expects double or float
    {
      // Processor uses float - direct processing
      std::vector<float*> in_ptrs(num_in_channels);
      std::vector<float*> out_ptrs(num_out_channels);


      for (int ch = 0; ch < num_in_channels; ++ch)
      {
        in_ptrs[ch] =(float*) input->getChannelData(ch);
        assert(in_ptrs[ch]);
      }

      for (int ch = 0; ch < num_out_channels; ++ch)
      {
        out_ptrs[ch] = output->channels[ch];
        assert(out_ptrs[ch]);
      }

      processor.process(implementation, std::span<float*>(in_ptrs), std::span<float*>(out_ptrs), num_samples);
    }
  }

  void setupParameters(TD::OP_ParameterManager* manager, void* reserved) override{
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

  std::string m_warn;

  void getWarningString(TD::OP_String *warning, void *reserved1)  override
  {
    if(!m_warn.empty())
      warning->setString(m_warn.c_str());
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
  }

  void
  buildDynamicMenu(const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info, void* reserved1) override
  {
  }


private:
  // Helper to update control values from TD parameters
  void update_controls(const TD::OP_Inputs* inputs){

  if constexpr(avnd::has_inputs<T>) {
    // Iterate over all parameter inputs and update from TD
    avnd::parameter_input_introspection<T>::for_all(
        avnd::get_inputs(implementation),
        [this, inputs]<typename Field>(Field& field) {
      constexpr auto name = touchdesigner::get_td_name<Field>();

      if constexpr(avnd::float_parameter<Field>)
      {
        field.value = static_cast<float>(inputs->getParDouble(name.data()));
      }
      else if constexpr(avnd::int_parameter<Field>)
      {
        field.value = inputs->getParInt(name.data());
      }
      else if constexpr(avnd::bool_parameter<Field>)
      {
        field.value = inputs->getParInt(name.data()) != 0;
      }
      else if constexpr(avnd::string_parameter<Field>)
      {
        const char* str = inputs->getParString(name.data());
        if(str)
          field.value.assign(str);
      }

      // Call update callback if it exists
      if_possible(field.update(implementation.effect));
        });
  }

  }
};
}
