#pragma once
#include <avnd/binding/ossia/configure.hpp>
#include <avnd/binding/ossia/port_setup.hpp>
#include <avnd/binding/ossia/port_run_preprocess.hpp>
#include <avnd/binding/ossia/port_run_postprocess.hpp>

#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/wrappers/bus_host_process_adapter.hpp>
#include <avnd/wrappers/channels_introspection.hpp>
#include <avnd/wrappers/control_display.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/midi_introspection.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <avnd/wrappers/midi_introspection.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/callbacks_adapter.hpp>
#include <avnd/wrappers/messages_introspection.hpp>
#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/for_each_in_tuple.hpp>

namespace oscr
{
/*
template <typename T>
class safe_node final
    : public ossia::nonowning_graph_node
    , public inlet_storage<T>
    , public outlet_storage<T>
{
  public:
    static constexpr int total_input_ports = avnd::total_input_count<T>();
    static constexpr int total_output_ports = avnd::total_input_count<T>();

    [[no_unique_address]]
    T impl;

    safe_node(ossia::exec_state_facade st) noexcept
    {
      m_inlets.reserve(total_input_ports);
      m_outlets.reserve(total_output_ports);

      // Initialize our own buffers
      // FIXME

      // Initialize internal effects buffer
      avnd::process_setup setup;
      setup.rate = st.sampleRate();
      setup.frames_per_buffer = st.bufferSize();
      // FIXME
      setup.input_channels = 2;
      setup.output_channels = 2;
      avnd::prepare(impl, setup);
    }
};
*/

template <typename T>
static void do_prepare(T& impl, ossia::exec_state_facade st, int new_ins, int new_outs)
{
  avnd::process_setup setup;
  setup.rate = st.sampleRate();
  setup.frames_per_buffer = st.bufferSize();
  // FIXME
  setup.input_channels = new_ins;
  setup.output_channels = new_outs;
  avnd::prepare(impl, setup);
}

/*
static void safety_checks(const ossia::token_request& tk, ossia::exec_state_facade st)
{
  auto [start, frames] = st.timings(tk);
  qDebug() <<" want: " << avnd::input_channels<T>(-1) << avnd::output_channels<T>(-1);
  qDebug() <<" have: " << actual_input_channels << actual_output_channels;
  qDebug() <<" port: " << audio_in.data.channels() << audio_out.data.channels();

  qDebug( ) << start << frames;

  for(auto& chan : audio_in.data) {
    qDebug() << "input: " << chan.size();
  }
  for(auto& chan : audio_out.data) {
    qDebug() << "output: " << chan.size();
  }
}
*/

// void process(double** in, double** out, int N)

struct ui_communication
{
  // Sends and receives info from the processor, e.g. audio signal, controls

  // Sets the controls from the UI
};

template <typename T>
struct builtin_arg_audio_ports
{
  void init(ossia::inlets& inlets, ossia::outlets& outlets) {}
};

template <typename T>
requires
   avnd::mono_per_sample_arg_processor<double, T>
|| avnd::monophonic_arg_audio_effect<double, T>
|| avnd::polyphonic_arg_audio_effect<double, T>
struct builtin_arg_audio_ports<T>
{
  ossia::audio_inlet in;
  ossia::audio_outlet out;

  void init(ossia::inlets& inlets, ossia::outlets& outlets)
  {
    inlets.push_back(&in);
    outlets.push_back(&out);
  }
};

template <typename T>
struct builtin_message_value_ports
{
  void init(ossia::inlets& inlets) {}
};

template <typename T>
requires (avnd::messages_introspection<T>::size > 0)
struct builtin_message_value_ports<T>
{
  ossia::value_inlet message_inlets[avnd::messages_introspection<T>::size];
  void init(ossia::inlets& inlets)
  {
    for(auto& in : message_inlets) {
      inlets.push_back(&in);
    }
  }
};

template <typename T>
class safe_node_base
        : public ossia::nonowning_graph_node
{
public:
  using processor_type = T;
  using inputs_t = typename avnd::inputs_type<T>::type;
  using outputs_t = typename avnd::outputs_type<T>::type;
  using param_in_info = avnd::parameter_input_introspection<T>;
  using midi_in_info = avnd::midi_input_introspection<T>;
  using midi_out_info = avnd::midi_output_introspection<T>;

  [[no_unique_address]]
  avnd::effect_container<T> impl;

  [[no_unique_address]]
  builtin_arg_audio_ports<T> audio_ports;

  [[no_unique_address]]
  builtin_message_value_ports<T> message_ports;

  [[no_unique_address]]
  inlet_storage<T> ossia_inlets;

  [[no_unique_address]]
  outlet_storage<T> ossia_outlets;

  [[no_unique_address]]
  avnd::process_adapter<T> processor;

  [[no_unique_address]]
  avnd::audio_channel_manager<T> channels;

  [[no_unique_address]]
  avnd::midi_storage<T> midi_buffers;

  [[no_unique_address]]
  avnd::control_storage<T> control_buffers;

  [[no_unique_address]]
  avnd::callback_storage<T> callbacks;

  static constexpr int total_input_ports = avnd::total_input_count<T>();
  static constexpr int total_output_ports = avnd::total_output_count<T>();

  int buffer_size{};
  double sample_rate{};

  safe_node_base(int buffer_size, double sample_rate)
    : channels{this->impl}
  {
    this->buffer_size = buffer_size;
    this->sample_rate = sample_rate;

    this->m_inlets.reserve(total_input_ports + 1);
    this->m_outlets.reserve(total_output_ports + 1);

    this->audio_ports.init(this->m_inlets, this->m_outlets);
    this->message_ports.init(this->m_inlets);

    // constexpr const int total_input_channels = avnd::input_channels<T>(-1);
    // constexpr const int total_output_channels = avnd::output_channels<T>(-1);

    channels.set_input_channels(this->impl, 0, 2);
    channels.set_output_channels(this->impl, 0, 2);

    // TODO
    this->impl.init_channels(2, 2);

    // Initialize the other ports
    this->finish_init();
  }

  template<typename Functor>
  void process_inputs(Functor& f, auto&& in)
  {
    using info = avnd::input_introspection<T>;
    [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        (
          f(boost::pfr::get<Index>(in), std::get<Index>(this->ossia_inlets.ports), avnd::num<Index>{}), ...
        );
    }(typename info::indices_n{});
  }
  template<typename Functor, typename M>
  void process_inputs(Functor& f, avnd::member_iterator<M>&& in)
  {
    for(auto& i : in)
      process_inputs(f, i);
  }
  template<typename Functor>
  void process_outputs(Functor& f, auto&& in)
  {
    using info = avnd::output_introspection<T>;
    [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        (
          f(boost::pfr::get<Index>(in), std::get<Index>(this->ossia_outlets.ports), avnd::num<Index>{}), ...
        );
    }(typename info::indices_n{});
  }
  template<typename Functor, typename M>
  void process_outputs(Functor& f, avnd::member_iterator<M>&& in)
  {
    for(auto& i : in)
      process_outputs(f, i);
  }

  template<typename Functor>
  void process_all_ports(Functor f)
  {
    if constexpr (avnd::inputs_type<T>::size > 0)
      process_inputs(f, impl.inputs());
    if constexpr (avnd::outputs_type<T>::size > 0)
      process_outputs(f, impl.outputs());
  }

  void initialize_all_ports()
  {
    // Setup inputs
    if constexpr (avnd::inputs_type<T>::size > 0)
    {
      using in_info = avnd::input_introspection<T>;
      using in_type = typename avnd::inputs_type<T>::type;
      auto& port_tuple = this->ossia_inlets.ports;

      [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
          setup_inlets<safe_node_base<T>> init{*this, this->m_inlets};
          (init(
            avnd::field_reflection<Index, boost::pfr::tuple_element_t<Index, in_type>>{}
            , std::get<Index>(port_tuple)
           ), ...
          );
      }(typename in_info::indices_n{});
    }

    // Setup outputs
    if constexpr (avnd::outputs_type<T>::size > 0)
    {
      using out_info = avnd::output_introspection<T>;
      using out_type = typename avnd::outputs_type<T>::type;
      auto& port_tuple = this->ossia_outlets.ports;

      [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
          setup_outlets<safe_node_base<T>> init{*this, this->m_outlets};
          (init(
            avnd::field_reflection<Index, boost::pfr::tuple_element_t<Index, out_type>>{}
            , std::get<Index>(port_tuple)
           ), ...
          );
      }(typename out_info::indices_n{});
    }
  }

  template<std::size_t Idx, typename R, typename... Args>
  struct do_callback
  {
    safe_node_base& self;
    R operator()(Args... args)
    {
      // Idx is the index of the port in the complete input array.
      // We need to map it to the callback index.
      ossia::value_outlet& port = std::get<Idx>(self.ossia_outlets.ports);
      if constexpr(sizeof...(Args) == 0)
        port.data.write_value(ossia::impulse{}, 0);
      else if constexpr(sizeof...(Args) == 1)
        port.data.write_value(args..., 0);

      if constexpr(!std::is_void_v<R>)
        return R{};
    }
  };

  void finish_init()
  {
    // Initialize the controls with their default values
    avnd::init_controls(this->impl.inputs());

    // Initialize the callbacks
    if constexpr(avnd::callback_introspection<outputs_t>::size > 0)
    {
      auto callbacks_initializer =
          [this] <typename Refl, template<typename...> typename L, typename... Args, std::size_t Idx>
          (std::string_view message, L<Args...>, Refl refl, avnd::num<Idx>) {
        return do_callback<Idx, typename Refl::return_type, Args...>{*this};
      };
      this->callbacks.wrap_callbacks(impl, callbacks_initializer);
    }

    // Initialize the other ports
    this->initialize_all_ports();

    this->audio_configuration_changed();
  }

  void audio_configuration_changed()
  {
    // qDebug() << "New Audio configuration: "
    //          << this->channels.actual_runtime_inputs
    //          << this->channels.actual_runtime_outputs;
    // Allocate buffers, setup everything
    avnd::process_setup setup_info{
        .input_channels = channels.actual_runtime_inputs,
        .output_channels = channels.actual_runtime_outputs,
        .frames_per_buffer = buffer_size,
        .rate = sample_rate};

    // This allocates the buffers that may be used for conversion
    // if e.g. we have an API that works with doubles,
    // and a plug-in that expects floats.
    // Here for instance we allocate buffers for an host that may invoke "process" with either floats or doubles.
    processor.allocate_buffers(setup_info, float{});
    processor.allocate_buffers(setup_info, double{});

    // Initialize the channels for the effect duplicator
    impl.init_channels(setup_info.input_channels, setup_info.output_channels);

    // Setup buffers for storing MIDI messages

    {
      midi_buffers.reserve_space(impl, buffer_size);
    }

    {
      control_buffers.reserve_space(impl, buffer_size);
    }

    // Effect-specific preparation
    avnd::prepare(impl, setup_info);
  }

  void set_channels(ossia::audio_port& port, int channels)
  {
    const int cur = port.channels();
    if(cur != channels)
    {
      // qDebug() << "Setting port channels: " << channels;
      port.set_channels(channels);
    }

    for(auto& chan : port)
    {
      if(chan.size() < this->buffer_size)
        chan.resize(this->buffer_size);
    }
  }

  struct audio_inlet_scan {
      safe_node_base& self;
      int k = 0;
      bool ok{true};

      void operator()(ossia::audio_inlet& in) noexcept {
        int expected = in.data.channels();
        self.channels.set_input_channels(self.impl, k, expected);
        int actual = self.channels.get_input_channels(self.impl, k);
        // qDebug() << "Scanning inlet " << k << ". Expected: " << expected << " ; actual: " << actual;
        ok &= (expected == actual);
        self.set_channels(in.data, actual);
        ++k;
      }
      void operator()(const auto& other) noexcept { }
  };
  struct audio_outlet_scan {
      safe_node_base& self;
      int k = 0;
      void operator()(ossia::audio_outlet& out) noexcept {
        int actual = self.channels.get_output_channels(self.impl, k);
        // qDebug() << "Scanning outlet " << k << ". Actual: " << actual;
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
    bool ok = std::apply([&] (auto&&... ports) {
      audio_inlet_scan match{*this};

      if constexpr(requires { this->audio_ports.in; })
        match(this->audio_ports.in);

      (match(ports), ...);
      return match.ok;
    }, ossia_inlets.ports);

    // Ensure that we have enough output space
    std::apply([&] (auto&&... ports) {
      audio_outlet_scan match{*this};

      if constexpr(requires { this->audio_ports.out; })
        match(this->audio_ports.out);

      (match(ports), ...);
    }, ossia_outlets.ports);

    bool changed = !ok;
    changed |= (current_input_channels != this->channels.actual_runtime_inputs);
    changed |= (current_output_channels != this->channels.actual_runtime_outputs);
    return changed;
  }

  bool prepare_run(int start, int frames)
  {
    // Check all the audio channels
    bool changed = scan_audio_input_channels();

    if(frames > this->buffer_size)
    {
      this->buffer_size = frames;
      changed = true;
    }
    if(changed)
    {
      audio_configuration_changed();
    }

    // Clean up MIDI output ports
    this->midi_buffers.clear_outputs(impl);

    // Clean up sample-accurate control output ports
    this->control_buffers.clear_outputs(impl);

    // Process inputs of all sorts
    process_all_ports(process_before_run<safe_node_base>{*this});

    // Process messages

    if constexpr (avnd::messages_type<T>::size > 0)
      process_messages();

    return true;
  }

  template <auto Idx, typename M>
  void invoke_message(const ossia::value& val, avnd::field_reflection<Idx, M>)
  {
    if constexpr (!std::is_void_v<avnd::message_reflection<M>>)
    {
      using refl = avnd::message_reflection<M>;
      constexpr auto arg_count = refl::count;
      constexpr auto f = M::func();

      if constexpr (arg_count == 0)
      {
        for(auto& m : this->impl.effects())
        {
          if constexpr (std::is_member_function_pointer_v<decltype(f)>)
            (m.*f)();
          else
            f();
        }
      }
      else if constexpr (arg_count == 1)
      {
        if constexpr (std::is_same_v<avnd::first_message_argument<M>, T&>)
        {
          for(auto& m : this->impl.effects())
          {
            if constexpr (std::is_member_function_pointer_v<decltype(f)>)
              (m.*f)(m);
            else
              f(m);
          }
        }
        else
        {
          //   FIXME ! oscquery_mapper.hpp already has all this code
          // using first_arg = boost::mp11::mp_first<typename refl::arguments>;
          // const auto& v = ossia::convert<std::decay_t<first_arg>>(val);
          // for(auto& m : this->impl.effects())
          // {
          //   if constexpr (std::is_member_function_pointer_v<decltype(f)>)
          //     (m.*f)(v);
          //   else
          //     f(v);
          // }
        }
      }
      else
      {
        // TODO use vecf, list, etc...
        // FIXME
      }
    }
    else
    {
      //FIXME > 1 arguments
    }
  }

  template <auto Idx, typename M>
  void process_message(avnd::field_reflection<Idx, M>)
  {
    ossia::value_inlet& inl = this->message_ports.message_inlets[Idx];
    if(inl.data.get_data().empty())
      return;
    for(const auto& val : inl.data.get_data())
    {
      invoke_message(val.value, avnd::field_reflection<Idx, M>{});
    }
  }

  void process_messages()
  {
    avnd::messages_introspection<T>::for_all([&] (auto m) {
      process_message(m);
    });
  }

  struct initialize_audio
  {
    const double** ins{};
    double** outs{};
    int k = 0;

    void operator()(const ossia::audio_inlet& in) noexcept {
      for(const ossia::audio_channel& c : in.data) {
        ins[k] = c.data();
        // qDebug() << "Init channel " << k << ". ins[k][0] ==  " << ins[k][0];
        k++;
      }
    }

    void operator()(ossia::audio_outlet& out) noexcept {
      for(ossia::audio_channel& c : out.data) {
        outs[k] = c.data();
        k++;
      }
    }

    void operator()(const auto& other) noexcept
    {
    }
  };

  void initialize_audio_arrays(const double** ins, double** outs)
  {
    std::apply([&] (auto&&... ports) {
      initialize_audio match{ins, outs, 0};

      if constexpr(requires { this->audio_ports.in; })
        match(this->audio_ports.in);

      (match(ports), ...);
    }, ossia_inlets.ports);

    std::apply([&] (auto&&... ports) {
      initialize_audio match{ins, outs, 0};

      if constexpr(requires { this->audio_ports.out; })
        match(this->audio_ports.out);

      (match(ports), ...);
    }, ossia_outlets.ports);
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
    const double** audio_ins = (const double**)alloca(sizeof(double*) * (1 + current_input_channels));
    double** audio_outs = (double**)alloca(sizeof(double*) * (1 + current_output_channels));
    initialize_audio_arrays(audio_ins, audio_outs);

    // Run
    processor.process(
        impl,
        avnd::span<double*>{const_cast<double**>(audio_ins), std::size_t(current_input_channels)},
        avnd::span<double*>{audio_outs, std::size_t(current_output_channels)},
        frames);

    this->finish_run();
  }

  void finish_run()
  {
     // Copy output events
     process_all_ports(process_after_run<safe_node_base>{*this});

     // Clean up MIDI inputs
     this->midi_buffers.clear_inputs(impl);

     // Clean up sample-accurate control input ports
     this->control_buffers.clear_inputs(impl);
  }

  std::string label() const noexcept override
  {
    return std::string{avnd::get_name<T>()};
  }
};

}
