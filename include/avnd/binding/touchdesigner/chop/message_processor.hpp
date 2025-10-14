#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/controls_double.hpp>
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
struct message_processor  : public TD::CHOP_CPlusPlusBase
{
  avnd::effect_container<T> implementation;

  parameter_setup<T> param_setup;
  explicit message_processor(const TD::OP_NodeInfo* info)
  {
    init();
  }

  void init()
  {
    avnd::init_controls(implementation);
  }

  void getGeneralInfo(TD::CHOP_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // No output: we're likely sending things over the network
    info->cookEveryFrame = avnd::bus_introspection<T>::output_busses == 0;
    info->cookEveryFrameIfAsked = true;
    info->inputMatchIndex = 0;
    info->timeslice = false;
  }

  bool getOutputInfo(TD::CHOP_OutputInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    if constexpr(avnd::has_outputs<T>)
    {
      info->numChannels = avnd::output_introspection<T>::size;
      info->numSamples = 1;
      info->sampleRate = 60;
      info->startIndex = 0;
    }
    else
    {
      info->numChannels = 0;
      info->numSamples = 0;
      info->sampleRate = 60;
      info->startIndex = 0;
    }
    return true;
  }

  void getChannelName(int32_t index, TD::OP_String* name, const TD::OP_Inputs* inputs, void* reserved) override
  {
    if(index < 0)
      return;
    if constexpr(avnd::has_outputs<T>)
    {
      avnd::output_introspection<T>::for_nth(index,
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
  }

  void execute(TD::CHOP_Output* output, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Update parameter values from TouchDesigner
    update_controls(inputs);

    // Execute process
    struct tick {
      int frames = 0;
    } t{.frames = output->numSamples};

    if(output->numSamples == 0)
      return;

    // Copy outputs in their respective channels
    invoke_effect(implementation, t);

    if(output->numChannels == avnd::output_introspection<T>::size)
    {
      int i = 0;
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&] <typename Field>(Field& port) {
        output->channels[i][0] = avnd::map_control_to_double<Field>(port.value);
      });
    }
    else
    {
      // TODO
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
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.menu(implementation, inputs, info);
  }

private:
  // Helper to update control values from TD parameters
  void update_controls(const TD::OP_Inputs* inputs){
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.update(implementation, inputs);
  }
};
}
