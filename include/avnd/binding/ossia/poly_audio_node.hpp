#pragma once
#include <avnd/binding/ossia/node.hpp>
namespace oscr
{

template<typename T>
concept ossia_compatible_nonaudio_processor =
    !(avnd::audio_argument_processor<T> || avnd::audio_port_processor<T>)
    ;

template<typename T>
concept ossia_compatible_audio_processor =
       avnd::poly_sample_array_input_port_count<double, T> > 0
    || avnd::poly_sample_array_output_port_count<double, T> > 0
;

template <typename T>
requires(!(
    real_mono_processor<float, T> || real_mono_processor<double, T> || ossia_compatible_audio_processor<T> || ossia_compatible_nonaudio_processor<T>))
class safe_node<T>
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
        avnd::span<double*>{audio_outs, init_audio_outs}, frames);

    this->finish_run();
  }
};


// Special case for the case which matches exactly the ossia situation
template <ossia_compatible_audio_processor T>
class safe_node<T>
    : public safe_node_base<T, safe_node<T>>
{
public:
  using in_busses = avnd::audio_bus_input_introspection<T>;
  using out_busses = avnd::audio_bus_output_introspection<T>;
  using safe_node_base<T, safe_node<T>>::safe_node_base;

  bool scan_audio_input_channels()
  {
    return false;
  }

  template<std::size_t NPred>
  int compute_output_channels(auto& field)
  {
    int actual_channels = 0;
    if constexpr(requires { field.channels(); })
    {
      // Fixed at compile-time
      actual_channels = field.channels();
    }
    else if constexpr(requires { field.request_channels; })
    {
      // Explicitly requested
      field.channels = field.request_channels;
      actual_channels = field.request_channels;
    }
    else if constexpr(requires { field.mimick_channel; })
    {
      // Mimicked from an input port
      auto& inputs = this->impl.inputs();
      auto& mimicked_port = (inputs.*field.mimick_channel);
      if constexpr(requires { mimicked_port.channels(); })
      {
        field.channels = mimicked_port.channels();
      }
      else if constexpr(requires { mimicked_port.channels; })
      {
        field.channels = mimicked_port.channels;
      }
      else
      {
        static_assert(decltype(field)::no_way_to_compute_the_output_channels);
      }
      actual_channels = field.channels;
    }
    else if constexpr(NPred == 0)
    {
      // Try to mimick the first audio port if everything else fails
      if constexpr(in_busses::size > 0)
      {
        auto& inputs = this->impl.inputs();
        auto& mimicked_port = in_busses::template get<0>(inputs);
        if constexpr(requires { mimicked_port.channels(); })
        {
          field.channels = mimicked_port.channels();
        }
        else if constexpr(requires { mimicked_port.channels; })
        {
          field.channels = mimicked_port.channels;
        }
        else
        {
          static_assert(decltype(field)::no_way_to_compute_the_output_channels);
        }
        actual_channels = field.channels;
      }
      else
      {
        static_assert(decltype(field)::no_way_to_compute_the_output_channels);
      }
    }
    return actual_channels;
  }

  OSSIA_MAXIMUM_INLINE
      void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    auto [start, frames] = st.timings(tk);

    // Compute channel count
    int total_input_channels = 0;
    in_busses::for_all_n2(this->impl.inputs(),
                          [&] <std::size_t NPred, std::size_t NField> (auto& field, avnd::predicate_index<NPred> pred_idx, avnd::field_index<NField> f_idx) {
      ossia::audio_inlet& port = tuplet::get<NField>(this->ossia_inlets.ports);
      total_input_channels += port->channels();
    });

    double** in_ptr = (double**)alloca(sizeof(double*) * total_input_channels);
    in_busses::for_all_n2(this->impl.inputs(),
                          [&] <std::size_t NPred, std::size_t NField> (auto& field, avnd::predicate_index<NPred> pred_idx, avnd::field_index<NField> f_idx) {
      ossia::audio_inlet& port = get<NField>(this->ossia_inlets.ports);
      auto cur_ptr = in_ptr;

      for(int i = 0; i < port->channels(); i++)
      {
        port->channel(i).resize(st.bufferSize());
        cur_ptr[i] = port->channel(i).data();
      }

      field.samples = cur_ptr;
      field.channels = port->channels();

      in_ptr += port->channels();
    });

    int output_channels = 0;
    out_busses::for_all_n2(this->impl.outputs(),
                          [&] <std::size_t NPred, std::size_t NField> (auto& field, avnd::predicate_index<NPred> pred_idx, avnd::field_index<NField> f_idx) {
      output_channels += this->template compute_output_channels<NPred>(field);
    });

    if(!this->prepare_run(tk, start, frames))
    {
      this->finish_run();
      return;
    }

    double** out_ptr = (double**)alloca(sizeof(double*) * output_channels);

    out_busses::for_all_n2(this->impl.outputs(),
                           [&] <std::size_t NPred, std::size_t NField> (auto& field, avnd::predicate_index<NPred> pred_idx, avnd::field_index<NField> f_idx) {
      ossia::audio_outlet& port = tuplet::get<NField>(this->ossia_outlets.ports);
      const int chans = this->template compute_output_channels<NPred>(field);
      port->set_channels(chans);

      auto cur_ptr = out_ptr;

      for(int i = 0; i < chans; i++)
      {
        port->channel(i).resize(st.bufferSize());
        cur_ptr[i] = port->channel(i).data();
      }

      field.samples = cur_ptr;

      out_ptr += chans;
    });


    avnd::invoke_effect(this->impl, frames);

    this->finish_run();
  }
};

// Special case for the easy non-audio case
template <ossia_compatible_nonaudio_processor T>
class safe_node<T>
    : public safe_node_base<T, safe_node<T>>
{
public:
  using safe_node_base<T, safe_node<T>>::safe_node_base;

  constexpr bool scan_audio_input_channels()
  {
    return false;
  }

  OSSIA_MAXIMUM_INLINE
      void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    auto [start, frames] = st.timings(tk);

    if(!this->prepare_run(tk, start, frames))
    {
      this->finish_run();
      return;
    }

    avnd::invoke_effect(this->impl, frames);

    this->finish_run();
  }
};
}
