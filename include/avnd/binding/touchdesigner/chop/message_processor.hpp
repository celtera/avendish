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
struct output_length_visitor
{
  template<avnd::parameter T>
  int64_t operator()(const T& port)
  {
    using value_type = std::decay_t<decltype(T::value)>;
    if constexpr(avnd::span_ish<value_type> && !avnd::string_ish<value_type>) {
      return std::ssize(port.value);
    }
    else
    {
      return 1;
    }
  }

  template<avnd::cpu_raw_buffer_port T>
  int64_t operator()(const T& port)
  {
    return port.buffer.bytesize;
  }

  template<avnd::cpu_typed_buffer_port T>
  int64_t operator()(const T& port)
  {
    return port.buffer.count;
  }

  template<avnd::callback T>
  int64_t operator()(const T& port)
  {
    // FIXME in the TD case we have to allocate and store the value that was sent
    return 0;
  }

  template<typename T>
  int64_t operator()(const T& port) = delete; // FIXME
};

struct output_write_visitor
{
  template<avnd::parameter T>
  void operator()(const T& port, float* channel, int64_t samples)
  {
    using value_type = std::decay_t<decltype(T::value)>;
    if constexpr(avnd::span_ish<value_type> && !avnd::string_ish<value_type>)
    {
      using vec_value_type = std::decay_t<decltype(port.value[0])>;
      if constexpr(std::is_arithmetic_v<vec_value_type>)
      {
        for(int i = 0, N = std::min(samples, (int64_t)port.value.size()); i < N; i++) {
          channel[i] = port.value[i];
        }
      }
      else
      {
        // TODO
      }
    }
    else
    {
      // samples > 0 is enforced before
      channel[0] = avnd::map_control_to_double<T>(port.value);
    }
  }

  template<avnd::cpu_raw_buffer_port T>
  void operator()(const T& port, float* channel, int64_t samples)
  {
    for(int i = 0, N = std::min(samples, (int64_t)port.buffer.bytesize); i < N; i++) {
      channel[i] = port.value.bytes[i];
    }
  }

  template<avnd::cpu_typed_buffer_port T>
  void operator()(const T& port, float* channel, int64_t samples)
  {
    for(int i = 0, N = std::min(samples, (int64_t)port.buffer.count); i < N; i++) {
      channel[i] = port.buffer.elements[i];
    }
  }

  template<avnd::callback T>
  void operator()(const T& port, float* channel, int64_t samples)
  {
    // FIXME in the TD case we have to allocate and store the value that was sent
    return ;
  }

  template<typename T>
  void operator()(const T& port, float* channel, int64_t samples) = delete; // FIXME
};

template <typename T>
struct message_processor : public TD::CHOP_CPlusPlusBase
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

    // Update parameter values from TouchDesigner
    update_controls(inputs);

    // Update main input values

    if constexpr(avnd::has_inputs<T>)
    {
      avnd::value_port_input_introspection<T>::for_all_n2(
          avnd::get_inputs(implementation),
          [&]<typename Field, std::size_t P, std::size_t I>(Field& field, avnd::predicate_index<P> pred_idx, avnd::field_index<I>) {

        auto chop_in = inputs->getInputCHOP(P);
        if(chop_in->numChannels > 0)
        {
          if(chop_in->numSamples > 0)
          {
            // Update value_port inputs
            parameter_update<T>{}.chop_in(field, chop_in);

            // Call update callback if it exists
            if_possible(field.update(implementation.effect));
          }
        }
      });
    }


    // Execute process
    const TD::OP_TimeInfo* ti = inputs->getTimeInfo();
    struct tick {
      int frames = 0;
    } t{.frames = (int) ti->deltaFrames};

    // Copy outputs in their respective channels
    invoke_effect(implementation, t);
  }

  bool getOutputInfo(TD::CHOP_OutputInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    auto ti = inputs->getTimeInfo();
    if constexpr(avnd::has_outputs<T>)
    {
      info->numChannels = avnd::output_introspection<T>::size;
      int64_t max_channel_length{0};
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&] (auto& field) {
        max_channel_length = std::max(max_channel_length, output_length_visitor{}(field));
      });
      info->numSamples = max_channel_length;
      info->sampleRate = ti->rate; // FIXME this should be the FPS / audio rate? how to get it ?
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
    if(output->numSamples <= 0)
      return;

    if constexpr(avnd::has_outputs<T>)
    {
      if(output->numChannels == avnd::output_introspection<T>::size)
      {
        avnd::output_introspection<T>::for_all_n(
            avnd::get_outputs(implementation),
            [&] <std::size_t I> (auto& field, avnd::field_index<I>) {
           output_write_visitor{}(field, output->channels[I], output->numSamples);
        });
      }
      else
      {
        // TODO
      }
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
