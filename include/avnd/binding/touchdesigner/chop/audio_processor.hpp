#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/common/export.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>

#include <cstring>
#include <span>
#include <string>

/**
 * TouchDesigner CHOP audio processor binding
 *
 * This file implements the wrapper for Avendish audio processors as TouchDesigner CHOPs
 * Following the pattern established in Max/PD bindings
 */

// Forward declare TD types (actual headers will be included in implementation)
namespace TD {
  class CHOP_CPlusPlusBase;
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

/**
 * Main audio processor wrapper template
 * T is the Avendish processor type
 */
template <typename T>
struct audio_processor
{
  // Channel configuration from Avendish introspection
  static constexpr int input_channels = avnd::input_channels<T>(1);
  static constexpr int output_channels = avnd::output_channels<T>(1);

  // Avendish components
  avnd::effect_container<T> implementation;
  avnd::process_adapter<T> processor;

  // TouchDesigner parameter handling
  parameter_setup<T> param_setup;

  // Runtime channel counts (can differ from static if dynamic)
  int runtime_input_count{input_channels};
  int runtime_output_count{output_channels};

  // Initialization
  void init(int argc, void* argv);
  void destroy();

  // TouchDesigner interface methods (will be pure virtual when inheriting from base)
  void getGeneralInfo(TD::CHOP_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved);
  bool getOutputInfo(TD::CHOP_OutputInfo* info, const TD::OP_Inputs* inputs, void* reserved);
  void getChannelName(int32_t index, TD::OP_String* name, const TD::OP_Inputs* inputs, void* reserved);
  void execute(TD::CHOP_Output* output, const TD::OP_Inputs* inputs, void* reserved);
  void setupParameters(TD::OP_ParameterManager* manager, void* reserved);

private:
  // Helper to update control values from TD parameters
  void update_controls(const TD::OP_Inputs* inputs);
};

/**
 * Metaclass for plugin registration
 * Manages DLL exports and instance creation
 */
template <typename T>
struct audio_processor_metaclass
{
  static inline audio_processor_metaclass* instance{};

  audio_processor_metaclass(); // Implements plugin registration
};

} // namespace touchdesigner::chop
