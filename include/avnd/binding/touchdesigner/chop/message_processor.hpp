#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/file_ports.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/binding/touchdesigner/info_output.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/tensor.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <halp/tensor_port.hpp>

#include <CHOP_CPlusPlusBase.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>


namespace touchdesigner::chop
{
struct output_length_visitor
{
  template<avnd::parameter_port T>
    requires (!avnd::tensor_port<T>)
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

  template <avnd::midi_port T>
  int64_t operator()(const T& port)
  {
    return 0; // FIXME what should we do for MIDI chops?
  }

  template<avnd::cpu_raw_buffer_port T>
  int64_t operator()(const T& port)
  {
    return port.buffer.byte_size;
  }

  template<avnd::cpu_typed_buffer_port T>
  int64_t operator()(const T& port)
  {
    return port.buffer.element_count;
  }

  template<avnd::tensor_port T>
  int64_t operator()(const T& port)
  {
    const auto& sh = avnd::shape_of(port.value);
    const std::size_t r = std::ranges::size(sh);
    if(r == 0)
      return 0;

    if constexpr(avnd::has_static_rank<T>)
    {
      constexpr std::size_t static_r = avnd::static_port_rank<T>();
      static_assert(static_r <= 2,
                    "TD CHOP cannot lower a tensor of rank > 2");
      if constexpr(static_r == 1)
      {
        return static_cast<int64_t>(*std::ranges::begin(sh));
      }
      else
      {
        auto it = std::ranges::begin(sh);
        ++it;
        return static_cast<int64_t>(*it);
      }
    }
    else
    {
      if(r == 1)
        return static_cast<int64_t>(*std::ranges::begin(sh));
      if(r == 2)
      {
        auto it = std::ranges::begin(sh);
        ++it;
        return static_cast<int64_t>(*it);
      }
      std::size_t total = 1;
      for(auto e : sh)
        total *= e;
      return static_cast<int64_t>(total);
    }
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

struct output_channel_count_visitor
{
  template<typename T>
  int64_t operator()(const T& port) const
  {
    if constexpr(avnd::tensor_port<T>)
    {
      const auto& sh = avnd::shape_of(port.value);
      const std::size_t r = std::ranges::size(sh);
      if(r == 0)
        return 1;
      if constexpr(avnd::has_static_rank<T>)
      {
        constexpr std::size_t static_r = avnd::static_port_rank<T>();
        static_assert(static_r <= 2,
                      "TD CHOP cannot lower a tensor of rank > 2.");
        if constexpr(static_r == 1)
          return 1;
        else
          return static_cast<int64_t>(*std::ranges::begin(sh));
      }
      else
      {
        if(r <= 1)
          return 1;
        if(r == 2)
          return static_cast<int64_t>(*std::ranges::begin(sh));
        return 1;
      }
    }
    else
    {
      return 1;
    }
  }
};

struct output_write_visitor
{
  template<avnd::parameter_port T>
    requires (!avnd::tensor_port<T>)
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
    for(int i = 0,
            N = std::min(samples, (int64_t)(port.buffer.byte_size / sizeof(float)));
        i < N; i++)
    {
      // FIXME we need some actual heuristic
      channel[i] = ((float*)port.buffer.raw_data)[i];
    }
  }

  template<avnd::cpu_typed_buffer_port T>
  void operator()(const T& port, float* channel, int64_t samples)
  {
    for(int i = 0, N = std::min(samples, port.buffer.element_count); i < N; i++) {
      channel[i] = port.buffer.elements[i];
    }
  }

  template<avnd::tensor_port T>
  void operator()(const T& port, float* channel, int64_t samples)
  {
    const auto* data = avnd::data_of(port.value);
    if(!data || samples <= 0)
      return;

    const auto& sh = avnd::shape_of(port.value);
    const std::size_t r = std::ranges::size(sh);
    if(r == 0)
      return;

    std::size_t total = 1;
    for(auto e : sh)
      total *= e;

    const int64_t N
        = std::min(samples, static_cast<int64_t>(total));
    for(int64_t i = 0; i < N; ++i)
      channel[i] = static_cast<float>(data[i]);
  }

  template <avnd::tensor_port T>
  void write_multi(
      const T& port, float* const* channel_base, int64_t channel_offset,
      int64_t samples) const
  {
    const auto* data = avnd::data_of(port.value);
    if(!data || samples <= 0)
      return;

    const auto& sh = avnd::shape_of(port.value);
    const std::size_t r = std::ranges::size(sh);
    if(r == 0)
      return;

    auto sh_it = std::ranges::begin(sh);
    const std::size_t rows = *sh_it;
    if(r == 1)
    {
      const int64_t N = std::min(samples, static_cast<int64_t>(rows));
      float* ch = channel_base[channel_offset];
      if(!ch)
        return;
      for(int64_t i = 0; i < N; ++i)
        ch[i] = static_cast<float>(data[i]);
      return;
    }

    ++sh_it;
    const std::size_t cols = *sh_it;
    std::size_t col_stride = cols;
    for(std::size_t d = 2; d < r; ++d)
    {
      ++sh_it;
      col_stride *= *sh_it;
    }

    const int64_t N = std::min(samples, static_cast<int64_t>(col_stride));
    for(std::size_t row = 0; row < rows; ++row)
    {
      float* ch = channel_base[channel_offset + static_cast<int64_t>(row)];
      if(!ch)
        continue;
      const auto* src = data + row * col_stride;
      for(int64_t i = 0; i < N; ++i)
        ch[i] = static_cast<float>(src[i]);
    }
  }

  template<avnd::callback T>
  void operator()(const T& port, float* channel, int64_t samples)
  {
    // FIXME in the TD case we have to allocate and store the value that was sent
    return ;
  }

  template <avnd::midi_port T>
  int64_t operator()(const T& port, float* channel, int64_t samples)
  {
    return 0; // FIXME what should we do for MIDI chops?
  }

  template <typename T>
  void operator()(const T& port, float* channel, int64_t samples) = delete; // FIXME
};

template <typename T>
struct message_processor;

template <typename T>
  requires(avnd::audio_processor<T>)
struct message_processor<T> : public TD::CHOP_CPlusPlusBase
{
  explicit message_processor(const TD::OP_NodeInfo* info) { }

  void execute(
      TD::CHOP_Output* outputs, const TD::OP_Inputs* inputs, void* reserved1) override
  {
  }
};

template <typename T>
  requires(!avnd::audio_processor<T>)
struct message_processor<T> : public TD::CHOP_CPlusPlusBase
{
  avnd::effect_container<T> implementation;

  parameter_setup<T> param_setup;
  touchdesigner::file_ports<T> file_setup;

  std::vector<std::vector<double>> tensor_input_scratch;

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

      constexpr std::size_t value_input_count
          = avnd::value_port_input_introspection<T>::size;
      avnd::tensor_port_input_introspection<T>::for_all_n2(
          avnd::get_inputs(implementation),
          [&]<typename Field, std::size_t P, std::size_t I>(
              Field& field, avnd::predicate_index<P> pred_idx,
              avnd::field_index<I>) {
        const int slot = static_cast<int>(value_input_count + P);
        if(slot >= inputs->getNumInputs())
          return;
        auto chop_in = inputs->getInputCHOP(slot);
        if(!chop_in || chop_in->numChannels <= 0 || chop_in->numSamples <= 0)
          return;

        this->template read_tensor_input<Field, P>(field, chop_in);
        if_possible(field.update(implementation.effect));
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
      int64_t total_channels{0};
      int64_t max_channel_length{0};
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&] (auto& field) {
        total_channels += output_channel_count_visitor{}(field);
        max_channel_length = std::max(max_channel_length, output_length_visitor{}(field));
      });
      info->numChannels = static_cast<int32_t>(total_channels);
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
      int32_t cursor = 0;
      bool resolved = false;
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&](auto& field) {
        if(resolved)
          return;
        using Field = std::remove_reference_t<decltype(field)>;
        const int64_t this_count = output_channel_count_visitor{}(field);
        if(index >= cursor && index < cursor + static_cast<int32_t>(this_count))
        {
          const int32_t sub = index - cursor;
          if constexpr(avnd::has_name<Field> || avnd::has_symbol<Field> || avnd::has_c_name<Field>)
          {
            static constexpr auto n = get_td_name<Field>();
            if(this_count > 1)
            {
              std::string base{n.data()};
              base.append("_ch");
              base.append(std::to_string(sub));
              name->setString(base.c_str());
            }
            else
            {
              name->setString(n.data());
            }
          }
          else
          {
            name->setString(fmt::format("chan%1", index + 1).c_str());
          }
          resolved = true;
        }
        else
        {
          cursor += static_cast<int32_t>(this_count);
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
      int32_t channel_cursor = 0;
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&](auto& field) {
        using Field = std::remove_reference_t<decltype(field)>;
        const int64_t this_count = output_channel_count_visitor{}(field);
        if(channel_cursor + this_count > output->numChannels)
          return;

        if constexpr(avnd::tensor_port<Field>)
        {
          output_write_visitor{}.write_multi(
              field, output->channels, channel_cursor, output->numSamples);
        }
        else
        {
          if(channel_cursor < output->numChannels)
            output_write_visitor{}(
                field, output->channels[channel_cursor], output->numSamples);
        }
        channel_cursor += static_cast<int32_t>(this_count);
      });
    }
  }

  void setupParameters(TD::OP_ParameterManager* manager, void* reserved) override
  {
    param_setup.setup(implementation, manager);
    file_setup.setup(implementation, manager);
  }

  int32_t
  getNumInfoCHOPChans(void *reserved1) override
  {
    return info_output<T>::get_num_info_chop_chans(implementation);
  }

  void
  getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved1) override
  {
    info_output<T>::get_info_chop_chan(implementation, index, chan);
  }

  bool
  getInfoDATSize(TD::OP_InfoDATSize* infoSize, void *reserved1) override
  {
    return info_output<T>::get_info_dat_size(implementation, infoSize);
  }

  void
  getInfoDATEntries(int32_t index, int32_t nEntries,
                    TD:: OP_InfoDATEntries* entries,
                    void *reserved1) override
  {
    info_output<T>::get_info_dat_entries(implementation, index, nEntries, entries);
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
    {
      parameter_update<T>{}.update(implementation, inputs);
      file_setup.load(implementation, inputs);
    }
  }

  template <typename Field, std::size_t P>
  void read_tensor_input(Field& field, const TD::OP_CHOPInput* chop_in)
  {
    using value_type = std::remove_cvref_t<decltype(field.value)>;

    const int n_chan = chop_in->numChannels;
    const int n_samp = chop_in->numSamples;
    if(n_chan <= 0 || n_samp <= 0)
      return;

    const bool single_channel = (n_chan == 1);
    std::vector<std::size_t> shape;
    if(single_channel)
      shape = {static_cast<std::size_t>(n_samp)};
    else
      shape = {static_cast<std::size_t>(n_chan),
               static_cast<std::size_t>(n_samp)};
    const std::size_t total = single_channel
                                  ? static_cast<std::size_t>(n_samp)
                                  : static_cast<std::size_t>(n_chan)
                                        * static_cast<std::size_t>(n_samp);

    if constexpr(avnd::view_tensor_like<value_type>)
    {
      using elem_t = avnd::tensor_element<value_type>;

      if constexpr(std::is_same_v<elem_t, double>)
      {
        // Per-port scratch buffer reused across ticks — no keep_alive
        // needed since the processor owns it.
        if(tensor_input_scratch.size() <= P)
          tensor_input_scratch.resize(P + 1);
        auto& scratch_raw = tensor_input_scratch[P];
        scratch_raw.resize(total);
        for(int c = 0; c < n_chan; ++c)
        {
          const float* src = chop_in->getChannelData(c);
          double* dst
              = scratch_raw.data() + static_cast<std::size_t>(c) * n_samp;
          for(int s = 0; s < n_samp; ++s)
            dst[s] = static_cast<double>(src[s]);
        }
        avnd::set_view_buffer(
            field.value, scratch_raw.data(), shape, {});
      }
      else
      {
        auto holder = std::make_shared<std::vector<elem_t>>(total);
        for(int c = 0; c < n_chan; ++c)
        {
          const float* src = chop_in->getChannelData(c);
          elem_t* dst
              = holder->data() + static_cast<std::size_t>(c) * n_samp;
          for(int s = 0; s < n_samp; ++s)
            dst[s] = static_cast<elem_t>(src[s]);
        }
        avnd::set_view_buffer(
            field.value, holder->data(), shape,
            std::shared_ptr<void>(holder, holder.get()));
      }
    }
    else if constexpr(avnd::resizable_tensor_like<value_type>)
    {
      using elem_t = avnd::tensor_element<value_type>;
      avnd::resize_to(field.value, shape);
      auto* dst = avnd::data_of(field.value);
      if(dst != nullptr)
      {
        for(int c = 0; c < n_chan; ++c)
        {
          const float* src = chop_in->getChannelData(c);
          elem_t* row
              = dst + static_cast<std::size_t>(c) * n_samp;
          for(int s = 0; s < n_samp; ++s)
            row[s] = static_cast<elem_t>(src[s]);
        }
      }
    }
    else if constexpr(avnd::mutable_tensor_like<value_type>)
    {
      using elem_t = avnd::tensor_element<value_type>;
      auto* dst = avnd::data_of(field.value);
      const auto& sh = avnd::shape_of(field.value);
      std::size_t cap = 1;
      for(auto e : sh)
        cap *= e;
      const std::size_t copyable = std::min(total, cap);
      if(dst != nullptr && copyable > 0)
      {
        std::size_t written = 0;
        for(int c = 0; c < n_chan && written < copyable; ++c)
        {
          const float* src = chop_in->getChannelData(c);
          for(int s = 0; s < n_samp && written < copyable; ++s, ++written)
            dst[written] = static_cast<elem_t>(src[s]);
        }
      }
    }
  }
};
}
