#pragma once
#include <avnd/binding/ossia/node.hpp>
namespace oscr
{
template <typename T>
requires(!(
    real_mono_processor<float, T> || real_mono_processor<double, T>)) class safe_node<T>
    : public safe_node_base<T, safe_node<T>>
{
public:
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
    bool ok = std::apply(
        [&](auto&&... ports)
        {
          audio_inlet_scan match{*this};

          if constexpr (requires { this->audio_ports.in; })
            match(this->audio_ports.in);

          (match(ports), ...);
          return match.ok;
        },
        this->ossia_inlets.ports);

    // Ensure that we have enough output space
    std::apply(
        [&](auto&&... ports)
        {
          audio_outlet_scan match{*this};

          if constexpr (requires { this->audio_ports.out; })
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
    int k = 0;

    void operator()(const ossia::audio_inlet& in) noexcept
    {
      for (const ossia::audio_channel& c : in.data)
      {
        ins[k] = c.data();
        k++;
      }
    }

    void operator()(ossia::audio_outlet& out) noexcept
    {
      for (ossia::audio_channel& c : out.data)
      {
        outs[k] = c.data();
        k++;
      }
    }

    void operator()(const auto& other) noexcept { }
  };

  void initialize_audio_arrays(const double** ins, double** outs)
  {
    std::apply(
        [&](auto&&... ports)
        {
          initialize_audio match{ins, outs, 0};

          if constexpr (requires { this->audio_ports.in; })
          {
            match(this->audio_ports.in);
          }

          (match(ports), ...);
        },
        this->ossia_inlets.ports);

    std::apply(
        [&](auto&&... ports)
        {
          initialize_audio match{ins, outs, 0};

          if constexpr (requires { this->audio_ports.out; })
          {
            match(this->audio_ports.out);
          }

          (match(ports), ...);
        },
        this->ossia_outlets.ports);
  }
  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    auto [start, frames] = st.timings(tk);

    if (!this->prepare_run(start, frames))
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

    this->initialize_audio_arrays(audio_ins, audio_outs);

    for (int i = 0; i < current_input_channels; i++)
      assert(audio_ins[i]);
    for (int i = 0; i < current_output_channels; i++)
      assert(audio_outs[i]);

    // Run
    this->processor.process(
        this->impl,
        avnd::span<double*>{
            const_cast<double**>(audio_ins), std::size_t(current_input_channels)},
        avnd::span<double*>{audio_outs, std::size_t(current_output_channels)},
        frames);

    this->finish_run();
  }
};
}
