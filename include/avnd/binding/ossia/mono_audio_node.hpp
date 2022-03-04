#pragma once
#include <avnd/binding/ossia/node.hpp>

namespace oscr
{
template<real_good_mono_processor T>
class safe_node<T> : public safe_node_base<T, safe_node<T>>
{
  public:
    using safe_node_base<T, safe_node<T>>::safe_node_base;

    ossia::audio_inlet& audio_inlet()
    {
      if constexpr(requires { this->audio_ports.in; })
        return this->audio_ports.in;
      else
        return std::get<ossia::audio_inlet>(this->ossia_inlets.ports);
    }
    ossia::audio_outlet& audio_outlet()
    {
      if constexpr(requires { this->audio_ports.out; })
        return this->audio_ports.out;
      else
        return std::get<ossia::audio_outlet>(this->ossia_outlets.ports);
    }

    bool scan_audio_input_channels()
    {
      const int current_input_channels = this->channels.actual_runtime_inputs;
      for(auto& chan : *audio_inlet())
      {
        if(chan.size() < this->buffer_size)
          chan.resize(this->buffer_size);
      }

      int port_input_channels = audio_inlet()->channels();
      int port_output_channels = audio_outlet()->channels();
      if(port_input_channels != current_input_channels || port_output_channels != current_input_channels)
      {
        this->channels.set_input_channels(this->impl, 0, port_input_channels);
        this->channels.set_output_channels(this->impl, 0, port_input_channels);
        this->channels.actual_runtime_inputs = port_input_channels;
        this->channels.actual_runtime_outputs = port_input_channels;

        this->set_channels(*audio_outlet(), port_input_channels);
        return true;
      }
      return false;
    }

    void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
    {
      auto [start, frames] = st.timings(tk);

      if (!this->prepare_run(start, frames))
      {
        this->finish_run();
        return;
      }

      ossia::audio_port& audio_in = *this->audio_inlet();
      ossia::audio_port& audio_out = *this->audio_outlet();

      // Initialize audio ports
      const int current_input_channels = this->channels.actual_runtime_inputs;
      const int current_output_channels = this->channels.actual_runtime_outputs;
      const double** audio_ins = (const double**)alloca(sizeof(double*) * (1 + current_input_channels));
      double** audio_outs = (double**)alloca(sizeof(double*) * (1 + current_output_channels));

      std::fill_n(audio_ins, 1 + current_input_channels, nullptr);
      std::fill_n(audio_outs, 1 + current_output_channels, nullptr);
      for(int i = 0; i < current_input_channels; i++)
      {
        audio_ins[i] = audio_in.channel(i).data();
        audio_outs[i] = audio_out.channel(i).data();
      }

      // Run
      this->processor.process(
                  this->impl
                  , avnd::span<double*>{const_cast<double**>(audio_ins), std::size_t(current_input_channels)}
                  , avnd::span<double*>{audio_outs, std::size_t(current_output_channels)}
                  , frames);

      this->finish_run();
    }
};

}
