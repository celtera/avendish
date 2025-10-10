#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/chop/audio_processor.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.impl.hpp>

// This file contains the implementation of CHOP audio processor methods
// It requires the actual TouchDesigner headers

#if __has_include("CHOP_CPlusPlusBase.h")

#include "CHOP_CPlusPlusBase.h"
#include "CPlusPlus_Common.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace touchdesigner::chop
{

// Implementation of audio_processor methods

template <typename T>
void audio_processor<T>::init(int argc, void* argv)
{
  // Initialize controls with default values
  avnd::init_controls(implementation);

  // Initialize polyphony if supported
  if constexpr (avnd::can_prepare<T>)
  {
    implementation.init_channels(input_channels, output_channels);
  }
}

template <typename T>
void audio_processor<T>::destroy()
{
  // Cleanup if needed
}

template <typename T>
void audio_processor<T>::getGeneralInfo(
    TD::CHOP_GeneralInfo* info,
    const TD::OP_Inputs* inputs,
    void* reserved)
{
  // Set cooking behavior
  info->cookEveryFrameIfAsked = false;

  // Match input 0 by default for channel/sample configuration
  info->inputMatchIndex = 0;

  // We're not using timeslice mode for now
  info->timeslice = false;
}

template <typename T>
bool audio_processor<T>::getOutputInfo(
    TD::CHOP_OutputInfo* info,
    const TD::OP_Inputs* inputs,
    void* reserved)
{
  // Get input CHOP if available
  const TD::OP_CHOPInput* input = inputs->getInputCHOP(0);

  if (input)
  {
    // Match input configuration
    info->numChannels = input->numChannels;
    info->numSamples = input->numSamples;
    info->sampleRate = static_cast<float>(input->sampleRate);
    info->startIndex = static_cast<uint32_t>(input->startIndex);
  }
  else
  {
    // No input, use defaults
    info->numChannels = output_channels;
    info->numSamples = 1;
    info->sampleRate = 44100.0f;
    info->startIndex = 0;
  }

  return true; // We specify output info
}

template <typename T>
void audio_processor<T>::getChannelName(
    int32_t index,
    TD::OP_String* name,
    const TD::OP_Inputs* inputs,
    void* reserved)
{
  // Generate channel names
  // TODO: Allow processor to specify custom names
  std::string chan_name = "chan" + std::to_string(index + 1);
  name->setString(chan_name.c_str());
}

template <typename T>
void audio_processor<T>::execute(
    TD::CHOP_Output* output,
    const TD::OP_Inputs* inputs,
    void* reserved)
{
  // Update parameter values from TouchDesigner
  update_controls(inputs);

  // Get input CHOP
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
  if constexpr (requires { processor.process(
      implementation,
      std::span<const double*>{},
      std::span<double*>{},
      num_samples); })
  {
    // Processor uses double - need conversion
    std::vector<std::vector<double>> in_buffers(num_in_channels);
    std::vector<std::vector<double>> out_buffers(num_out_channels);
    std::vector<const double*> in_ptrs(num_in_channels);
    std::vector<double*> out_ptrs(num_out_channels);

    // Convert input to double
    for (int ch = 0; ch < num_in_channels; ++ch)
    {
      in_buffers[ch].resize(num_samples);
      for (int s = 0; s < num_samples; ++s)
      {
        in_buffers[ch][s] = static_cast<double>(input->getChannelData(ch)[s]);
      }
      in_ptrs[ch] = in_buffers[ch].data();
    }

    // Allocate output buffers
    for (int ch = 0; ch < num_out_channels; ++ch)
    {
      out_buffers[ch].resize(num_samples);
      out_ptrs[ch] = out_buffers[ch].data();
    }

    // Process
    processor.process(
        implementation,
        std::span<const double*>{in_ptrs.data(), static_cast<size_t>(num_in_channels)},
        std::span<double*>{out_ptrs.data(), static_cast<size_t>(num_out_channels)},
        num_samples);

    // Convert output to float
    for (int ch = 0; ch < num_out_channels; ++ch)
    {
      for (int s = 0; s < num_samples; ++s)
      {
        output->channels[ch][s] = static_cast<float>(out_buffers[ch][s]);
      }
    }
  }
  else if constexpr (requires { processor.process(
      implementation,
      std::span<const float*>{},
      std::span<float*>{},
      num_samples); })
  {
    // Processor uses float - direct processing
    std::vector<const float*> in_ptrs(num_in_channels);
    std::vector<float*> out_ptrs(num_out_channels);

    for (int ch = 0; ch < num_in_channels; ++ch)
    {
      in_ptrs[ch] = input->getChannelData(ch);
    }

    for (int ch = 0; ch < num_out_channels; ++ch)
    {
      out_ptrs[ch] = output->channels[ch];
    }

    processor.process(
        implementation,
        std::span<const float*>{in_ptrs.data(), static_cast<size_t>(num_in_channels)},
        std::span<float*>{out_ptrs.data(), static_cast<size_t>(num_out_channels)},
        num_samples);
  }
  else
  {
    // Try calling operator() directly with sample count
    if constexpr (requires { implementation.effect.operator()(num_samples); })
    {
      // Need to set up input/output references in implementation first
      // This is a simpler case - just call operator()
      implementation.effect.operator()(num_samples);

      // Copy results to output
      // TODO: Map Avendish outputs to TD outputs properly
    }
  }
}

template <typename T>
void audio_processor<T>::setupParameters(TD::OP_ParameterManager* manager, void* reserved)
{
  param_setup.setup(manager);
}

template <typename T>
void audio_processor<T>::update_controls(const TD::OP_Inputs* inputs)
{
  // Iterate over all parameter inputs and update from TD
  avnd::parameter_input_introspection<T>::for_all(
      avnd::get_inputs(implementation.effect),
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
        field.value = std::string(str);
    }

    // Call update callback if it exists
    if_possible(field.update(implementation.effect));
      });
}

} // namespace touchdesigner::chop

#endif // has CHOP_CPlusPlusBase.h
