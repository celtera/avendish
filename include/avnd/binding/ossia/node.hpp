#pragma once
#include <avnd/binding/ossia/configure.hpp>
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
struct exec_inlet_init
{
  ossia::inlets& ins;
  int inlet = 0;

  void operator()(const avnd::audio_port auto& in)
  {
    auto p = new Process::AudioInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  void operator()(const avnd::midi_port auto& in)
  {
    auto p = new Process::MidiInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  template<avnd::parameter T>
  void operator()(const T& in)
  {
    if constexpr(avnd::control<T>)
    {
      constexpr auto ctl = oscr::make_control_in<T>();
      if(auto p = ctl.create_inlet(Id<Process::Port>(inlet++), &self))
      {
        p->hidden = true;
        ins.push_back(p);
      }
    }
    else
    {
      auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
      setupNewPort(in, p);
      ins.push_back(p);
    }
  }
#if SCORE_PLUGIN_GFX
  void operator()(const avnd::texture_port auto& in)
  {
    auto p = new Gfx::TextureInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
#endif

  void operator()(const auto& ctrl)
  {
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};

struct exec_outlet_init
{
    ossia::outlets& outs;
    int outlet = 0;
  void operator()(const avnd::audio_port auto& out)
  {
    auto p = new Process::AudioOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    if (outlet == 1)
      p->setPropagate(true);
    outs.push_back(p);
  }
  void operator()(const avnd::midi_port auto& out)
  {
    auto p = new Process::MidiOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }
  template<avnd::parameter T>
  void operator()(const T& out)
  {
    if constexpr(avnd::control<T>)
    {
      constexpr auto ctl = oscr::make_control_out<T>();
      if(auto p = ctl.create_inlet(Id<Process::Port>(outlet++), &self))
      {
        p->hidden = true;
        outs.push_back(p);
      }
    }
    else
    {
      auto p = new Process::ValueOutlet(Id<Process::Port>(outlet++), &self);
      setupNewPort(out, p);
      outs.push_back(p);
    }
  }
#if SCORE_PLUGIN_GFX
  void operator()(const avnd::texture_port auto& out)
  {
    auto p = new Gfx::TextureOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }
#endif
  void operator()(const auto& ctrl)
  {
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};
*/

// Compile-time map of avnd concept to ossia port
template <typename T>
struct get_ossia_inlet_type;
template <avnd::audio_port T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::audio_inlet;
};
template <avnd::parameter T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::value_inlet;
};
template <avnd::midi_port T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::midi_inlet;
};
template <avnd::texture_port T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::texture_inlet;
};

template <typename T>
using get_ossia_inlet_type_t = typename get_ossia_inlet_type<T>::type;

template <typename T>
struct get_ossia_outlet_type;
template <avnd::audio_port T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::audio_outlet;
};
template <avnd::parameter T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::value_outlet;
};
template <avnd::midi_port T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::midi_outlet;
};
template <avnd::texture_port T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::texture_outlet;
};
template <avnd::callback T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::value_outlet;
};

template <typename T>
using get_ossia_outlet_type_t = typename get_ossia_outlet_type<T>::type;

template <typename T>
struct refl
{
  using inputs_t = typename avnd::inputs_type<T>::type;
  using outputs_t = typename avnd::outputs_type<T>::type;
  using in_info = avnd::input_introspection<T>;
  using out_info = avnd::output_introspection<T>;
  using param_in_info = avnd::parameter_input_introspection<T>;
  using param_out_info = avnd::parameter_output_introspection<T>;
  using midi_in_info = avnd::midi_input_introspection<T>;
  using midi_out_info = avnd::midi_output_introspection<T>;
  using raw_midi_out_info = avnd::raw_container_midi_output_introspection<T>;
  using audio_bus_in_info = avnd::audio_bus_input_introspection<T>;
  using audio_bus_out_info = avnd::audio_bus_output_introspection<T>;
  using audio_channel_in_info = avnd::audio_channel_input_introspection<T>;
  using audio_channel_out_info = avnd::audio_channel_output_introspection<T>;
};

template <typename T>
struct inlet_storage
{
  // Type of the "inputs" struct:
  // struct inputs {
  //   struct a: audio_in { } a;
  //   struct b: audio_out { } b;
  //   struct c: control_inlet { float value; } c;
  // };
  using inputs_type = typename avnd::inputs_type<T>::type;

  // tuple<a, b, c>
  using inputs_tuple = typename avnd::inputs_type<T>::tuple;

  // tuple<ossia::audio_inlet, ossia::audio_outlet, ossia::value_inlet>
  using ossia_inputs_tuple
      = boost::mp11::mp_transform<get_ossia_inlet_type_t, inputs_tuple>;
  ossia_inputs_tuple ports;
  /*
  // mp_list<mp_size<0>, mp_size<1>, mp_size<2>>;
  using inputs_indices = boost::mp11::mp_iota_c<boost::mp11::mp_size<inputs_tuple>::value>;


  // Is the port at index N, a control input
  template<class N>
  using check_control_input = is_control_input<boost::mp11::mp_at_c<inputs_tuple, N::value>>;

  // mp_list<mp_size<2>>
  using control_input_indices = boost::mp11::mp_copy_if<inputs_indices, check_control_input>;

  // control_input_index<0>::value == 2
  template<std::size_t ControlN>
  using control_input_index = boost::mp11::mp_at_c<control_input_indices, ControlN>;

  // tuple<c>
  using control_input_tuple = boost::mp11::mp_copy_if<inputs_tuple, is_control_input>;

  // tuple<float>
  using control_input_values_type = typename oscr::get_control_type_list<control_input_tuple>::type;

  static constexpr auto inlet_size = std::tuple_size_v<inputs_tuple>;
  static constexpr auto audio_in_count = boost::mp11::mp_count_if<inputs_tuple, IsAudioInput>::value;
  static constexpr auto value_in_count = boost::mp11::mp_count_if<inputs_tuple, IsValueInput>::value;
  static constexpr auto midi_in_count = boost::mp11::mp_count_if<inputs_tuple, IsMidiInput>::value;
  static constexpr auto texture_in_count = boost::mp11::mp_count_if<inputs_tuple, IsTextureInput>::value;
  static constexpr auto control_in_count = boost::mp11::mp_count_if<inputs_tuple, IsControlInput>::value;
  */
};

template <typename T>
struct outlet_storage
{
  using outputs_type = typename avnd::outputs_type<T>::type;

  using outputs_tuple = typename avnd::outputs_type<T>::tuple;

  using ossia_outputs_tuple
      = boost::mp11::mp_transform<get_ossia_outlet_type_t, outputs_tuple>;
  ossia_outputs_tuple ports;
};

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

template <typename Exec_T>
struct InitInlets
{
  Exec_T& self;
  ossia::inlets& inlets;

  /*
  void operator()(AudioInput auto& in, ossia::audio_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(MultichannelAudioInput<decltype(in)>) {
      in.samples.buffer = std::addressof(port->get());
    }
    else if constexpr(PortAudioInput<decltype(in)>) {
      in.port = std::addressof(*port);
    }
  }

  void operator()(PortValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(requires { in.is_event(); }) {
      port->is_event = in.is_event();
    }
    in.port = std::addressof(*port);
  }

  void operator()(TimedValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(requires { in.is_event(); }) {
      port->is_event = in.is_event();
    }
  }

  void operator()(SingleValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(requires { in.is_event(); }) {
      port->is_event = in.is_event();
    }
  }

  void operator()(PortMidiInput auto& in, ossia::midi_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    in.port = std::addressof(*port);
  }
  void operator()(MessagesMidiInput auto& in, ossia::midi_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  void operator()(TextureInput auto& in, ossia::texture_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  void operator()(ControlInput auto& ctrl, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    port->is_event = true;

    if constexpr(requires { ctrl.port; })
      ctrl.port = std::addressof(*port);

    get_control(ctrl).setup_exec(port);
  }
  */
};

template <typename Exec_T>
struct InitOutlets
{
  Exec_T& self;
  ossia::outlets& outlets;
  /*
  void operator()(AudioOutput auto& out, ossia::audio_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(MultichannelAudioOutput<decltype(out)>) {
      out.samples.buffer = std::addressof(port->get());
    }
    else if constexpr(PortAudioOutput<decltype(out)>) {
      out.port = std::addressof(*port);
    }
  }

  void operator()(PortValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    out.port = std::addressof(*port);

    if constexpr(requires { out.type; }) {
      if(!out.type.empty())
        port->type = ossia::parse_dataspace(out.type);
    }
  }

  void operator()(TimedValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(requires { out.type; }) {
      if(!out.type.empty())
        port->type = ossia::parse_dataspace(out.type);
    }
  }

  void operator()(SingleValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(requires { out.type; }) {
      if(!out.type.empty())
        port->type = ossia::parse_dataspace(out.type);
    }
  }

  void operator()(MidiOutput auto& out, ossia::midi_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    out.port = std::addressof(*port);
  }

  void operator()(TextureOutput auto& out, ossia::texture_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  void operator()(ControlOutput auto& ctrl, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(requires { ctrl.port; })
      ctrl.port = std::addressof(*port);

    ctrl.display().setup_exec(port);
  }
  */
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
class safe_node_base
        : public ossia::nonowning_graph_node
{
public:
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

    // constexpr const int total_input_channels = avnd::input_channels<T>(-1);
    // constexpr const int total_output_channels = avnd::output_channels<T>(-1);

    channels.set_input_channels(this->impl, 0, 2);
    channels.set_output_channels(this->impl, 0, 2);

    // TODO
    this->impl.init_channels(2, 2);

    // Initialize the other ports
    this->finish_init();
  }

  void initialize_all_ports()
  {
    if constexpr (avnd::inputs_type<T>::size > 0)
    {
      // const auto& ins = this->impl.inputs();
      // const auto& proc_tuple = boost::pfr::detail::tie_as_tuple(ins);
      auto& port_tuple = this->ossia_inlets.ports;
      std::apply(
          [this](auto&&... port) { (this->m_inlets.push_back(&port), ...); },
          port_tuple);
    }
    if constexpr (avnd::outputs_type<T>::size > 0)
    {
      // const auto& outs = this->impl.outputs();
      // const auto& proc_tuple = boost::pfr::detail::tie_as_tuple(outs);
      auto& port_tuple = this->ossia_outlets.ports;
      std::apply(
          [this](auto&&... port) { (this->m_outlets.push_back(&port), ...); },
          port_tuple);
    }
  }

  void finish_init()
  {
    // Initialize the controls with their default values
    avnd::init_controls(this->impl.inputs());

    // Initialize the callbacks
    if constexpr(avnd::callback_introspection<outputs_t>::size > 0)
    {
      auto callbacks_initializer =
          [this] <typename Refl, template<typename...> typename L, typename... Args>
          (std::string_view message, L<Args...>, Refl refl) {
        using return_type = typename Refl::return_type;
        // TODO!
        if constexpr(std::is_void_v<return_type>)
          return [this] (Args...) { return; };
        else
          return [this] (Args...) { return return_type{}; };
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

  void process_input_controls()
  {
    // Here, copy the input controls to the ports in the object
    /* CUSTOMIZATION POINT */
  }

  void process_input_events()
  {
    // Here, copy the input midi events to the relevant ports in the object
    /* CUSTOMIZATION POINT */
  }

  void process_output_controls()
  {
    // Here, copy the output controls to the ports in the object
    // (e.g. db feedback, etc...)
    /* CUSTOMIZATION POINT */
  }

  void process_output_events()
  {
    // Here, copy the output MIDI events to the ports in the object
    /* CUSTOMIZATION POINT */
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
    process_input_controls();

    process_input_events();

    return true;
  }

  struct initialize_audio {
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
      return;

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
     process_output_controls();

     process_output_events();

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
template <typename T>
class safe_node;

}
