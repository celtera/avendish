#pragma once
#include <avnd/binding/ossia/node.hpp>
namespace oscr
{

template <typename T>
  requires(!(
      real_good_mono_processor<T> || mono_generator<T>
      || ossia_compatible_audio_processor<T> || ossia_compatible_nonaudio_processor<T>))
class safe_node<T> : public safe_node_base<T, safe_node<T>>
{
public:
  [[no_unique_address]] avnd::process_adapter<T> processor;

  using safe_node_base<T, safe_node<T>>::safe_node_base;

  struct audio_inlet_scan
  {
    safe_node& self;
    int k = 0;
    bool ok{true};

    void operator()(ossia::audio_inlet& in) noexcept
    {
      int expected = in.data.channels();
      self.channels.set_input_channels(self.impl, k, expected);
      int actual = self.channels.get_input_channels(self.impl, k);
      ok &= (expected == actual);
      self.set_channels(in.data, actual);
      ++k;
    }
    void operator()(const auto& other) noexcept { }
  };
  struct audio_outlet_scan
  {
    safe_node& self;
    int k = 0;
    void operator()(ossia::audio_outlet& out) noexcept
    {
      int actual = self.channels.get_output_channels(self.impl, k);
      self.set_channels(out.data, actual);
      ++k;
    }
    void operator()(const auto& other) noexcept { }
  };

  bool scan_audio_input_channels()
  {
    const int current_input_channels = this->channels.actual_runtime_inputs;
    const int current_output_channels = this->channels.actual_runtime_outputs;

    // Scan the input channels
    bool ok = tuplet::apply(
        [&](auto&&... ports) {
      audio_inlet_scan match{*this};

      if constexpr(requires { this->audio_ports.in; })
        match(this->audio_ports.in);

      (match(ports), ...);
      return match.ok;
        },
        this->ossia_inlets.ports);

    // Ensure that we have enough output space
    tuplet::apply(
        [&](auto&&... ports) {
      audio_outlet_scan match{*this};

      if constexpr(requires { this->audio_ports.out; })
        match(this->audio_ports.out);

      (match(ports), ...);
        },
        this->ossia_outlets.ports);

    bool changed = !ok;
    changed |= (current_input_channels != this->channels.actual_runtime_inputs);
    changed |= (current_output_channels != this->channels.actual_runtime_outputs);
    return changed;
  }

  struct initialize_audio
  {
    const double** ins{};
    double** outs{};
    std::size_t in_n{}, out_n{};
    int k = 0;

    void operator()(const ossia::audio_inlet& in) noexcept
    {
      for(const ossia::audio_channel& c : in.data)
      {
        if(k < in_n)
          ins[k++] = c.data();
      }
    }

    void operator()(ossia::audio_outlet& out) noexcept
    {
      for(ossia::audio_channel& c : out.data)
      {
        if(k < out_n)
          outs[k++] = c.data();
      }
    }

    void operator()(const auto& other) noexcept { }
  };

  void initialize_audio_arrays(
      const double** ins, double** outs, std::size_t kin, std::size_t kout)
  {
    tuplet::apply(
        [&](auto&&... ports) {
      initialize_audio match{ins, outs, kin, kout, 0};

      if constexpr(requires { this->audio_ports.in; })
      {
        match(this->audio_ports.in);
      }

      (match(ports), ...);
        },
        this->ossia_inlets.ports);

    tuplet::apply(
        [&](auto&&... ports) {
      initialize_audio match{ins, outs, kin, kout, 0};

      if constexpr(requires { this->audio_ports.out; })
      {
        match(this->audio_ports.out);
      }

      (match(ports), ...);
        },
        this->ossia_outlets.ports);
  }

  OSSIA_MAXIMUM_INLINE
  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    // FIXME start isn't handled
    auto [start, frames] = st.timings(tk);

    if(!this->prepare_run(tk, start, frames))
    {
      this->finish_run();
      return;
    }

    // Initialize audio ports
    const int current_input_channels = this->channels.actual_runtime_inputs;
    const int current_output_channels = this->channels.actual_runtime_outputs;
    const double** audio_ins
        = (const double**)alloca(sizeof(double*) * (1 + current_input_channels));
    double** audio_outs
        = (double**)alloca(sizeof(double*) * (1 + current_output_channels));

    std::fill_n(audio_ins, 1 + current_input_channels, nullptr);
    std::fill_n(audio_outs, 1 + current_output_channels, nullptr);

    this->initialize_audio_arrays(
        audio_ins, audio_outs, current_input_channels, current_output_channels);

    std::size_t init_audio_ins = 0;
    std::size_t init_audio_outs = 0;
    for(int i = 0; i < current_input_channels; i++)
      init_audio_ins += bool(audio_ins[i]) ? 1 : 0;
    for(int i = 0; i < current_output_channels; i++)
      init_audio_outs += bool(audio_outs[i]) ? 1 : 0;

    // Run
    this->processor.process(
        this->impl, avnd::span<double*>{const_cast<double**>(audio_ins), init_audio_ins},
        avnd::span<double*>{audio_outs, init_audio_outs}, tick_info{tk, st, frames},
        this->smooth);

    this->finish_run();
  }
};

}
