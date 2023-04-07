#pragma once
#include <avnd/binding/ossia/configure.hpp>
#include <avnd/binding/ossia/ffts.hpp>
#include <avnd/binding/ossia/port_run_postprocess.hpp>
#include <avnd/binding/ossia/port_run_preprocess.hpp>
#include <avnd/binding/ossia/port_setup.hpp>
#include <avnd/binding/ossia/soundfiles.hpp>
#include <avnd/binding/ossia/time_controls.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/midi.hpp>
#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/wrappers/bus_host_process_adapter.hpp>
#include <avnd/wrappers/callbacks_adapter.hpp>
#include <avnd/wrappers/control_display.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/smooth.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
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

template <typename T>
struct builtin_arg_audio_ports
{
  void init(ossia::inlets& inlets, ossia::outlets& outlets) { }
};

template <avnd::audio_argument_processor T>
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
  void init(ossia::inlets& inlets) { }
};

template <typename T>
  requires(avnd::messages_introspection<T>::size > 0)
struct builtin_message_value_ports<T>
{
  ossia::value_inlet message_inlets[avnd::messages_introspection<T>::size];
  void init(ossia::inlets& inlets)
  {
    for(auto& in : message_inlets)
    {
      inlets.push_back(&in);
    }
  }
};

template <typename Field>
using controls_type = std::decay_t<decltype(Field::value)>;

template <typename T>
using atomic_shared_ptr = boost::atomic_shared_ptr<T>;

template <typename T>
struct controls_mirror
{
  static constexpr int i_size = avnd::control_input_introspection<T>::size;
  static constexpr int o_size = avnd::control_output_introspection<T>::size;
  using i_tuple
      = avnd::filter_and_apply<controls_type, avnd::control_input_introspection, T>;
  using o_tuple
      = avnd::filter_and_apply<controls_type, avnd::control_output_introspection, T>;

  controls_mirror()
  {
    inputs.load(new i_tuple);
    outputs.load(new o_tuple);
  }

  [[no_unique_address]] atomic_shared_ptr<i_tuple> inputs;
  [[no_unique_address]] atomic_shared_ptr<o_tuple> outputs;

  std::bitset<i_size> inputs_bits;
  std::bitset<o_size> outputs_bits;
};

template <typename T>
struct controls_input_queue
{
};
template <typename T>
struct controls_output_queue
{
};
template <typename T>
  requires(avnd::control_input_introspection<T>::size > 0)
struct controls_input_queue<T>
{
  static constexpr int i_size = avnd::control_input_introspection<T>::size;
  using i_tuple
      = avnd::filter_and_apply<controls_type, avnd::control_input_introspection, T>;

  moodycamel::ConcurrentQueue<i_tuple> ins_queue;
  std::bitset<i_size> inputs_set;
};

template <typename T>
  requires(avnd::control_output_introspection<T>::size > 0)
struct controls_output_queue<T>
{
  static constexpr int o_size = avnd::control_output_introspection<T>::size;
  using o_tuple
      = avnd::filter_and_apply<controls_type, avnd::control_output_introspection, T>;

  moodycamel::ConcurrentQueue<o_tuple> outs_queue;

  std::bitset<o_size> outputs_set;
};

template <typename T>
struct controls_queue
    : controls_input_queue<T>
    , controls_output_queue<T>
{
};

template <typename Self, std::size_t Idx, typename Field, typename R, typename... Args>
struct do_callback;

template <typename Self, std::size_t Idx, typename Field, typename R, typename... Args>
  requires(!requires { Field::timestamp; })
struct do_callback<Self, Idx, Field, R, Args...>
{
  Self& self;
  Field& field;
  R operator()(Args... args)
  {
    // Idx is the index of the port in the complete input array.
    // We need to map it to the callback index.
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    if constexpr(sizeof...(Args) == 0)
    {
      port.data.write_value(ossia::impulse{}, 0);
    }
    else if constexpr(sizeof...(Args) == 1)
    {
      port.data.write_value(to_ossia_value(field, args...), 0);
    }
    else
    {
      std::vector<ossia::value> vec{to_ossia_value(field, args)...};
      port.data.write_value(std::move(vec), 0);
    }

    if constexpr(!std::is_void_v<R>)
      return R{};
  }
};

template <typename Self, std::size_t Idx, typename Field, typename R, typename... Args>
  requires(requires { Field::timestamp; })
struct do_callback<Self, Idx, Field, R, Args...>
{
  Self& self;
  Field& field;

  void do_call(int64_t ts)
  {
    // Idx is the index of the port in the complete input array.
    // We need to map it to the callback index.
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    port.data.write_value(ossia::impulse{}, ts);
  }

  template <typename U>
  void do_call(int64_t ts, U&& u)
  {
    // Idx is the index of the port in the complete input array.
    // We need to map it to the callback index.
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    port.data.write_value(to_ossia_value(field, std::forward<U>(u)), ts);
  }

  template <typename... U>
  void do_call(int64_t ts, U&&... us)
  {
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    std::vector<ossia::value> vec{to_ossia_value(field, us)...};
    port.data.write_value(std::move(vec), ts);
  }

  R operator()(Args... args)
  {
    do_call(static_cast<Args&&>(args)...);
    if constexpr(!std::is_void_v<R>)
      return R{};
  }
};

template <typename T>
class safe_node_base_base : public ossia::nonowning_graph_node
{
public:
  safe_node_base_base(int buffer_size, double sample_rate)
      : channels{this->impl}
  {
  }

  int buffer_size{};
  double sample_rate{};
  double tempo{ossia::root_tempo};

  [[no_unique_address]] avnd::effect_container<T> impl;

  [[no_unique_address]] oscr::builtin_arg_audio_ports<T> audio_ports;

  [[no_unique_address]] oscr::builtin_message_value_ports<T> message_ports;

  [[no_unique_address]] oscr::inlet_storage<T> ossia_inlets;

  [[no_unique_address]] oscr::outlet_storage<T> ossia_outlets;

  [[no_unique_address]] avnd::audio_channel_manager<T> channels;

  [[no_unique_address]] avnd::midi_storage<T> midi_buffers;

  [[no_unique_address]] avnd::control_storage<T> control_buffers;

  [[no_unique_address]] oscr::time_control_storage<T> time_controls;

  [[no_unique_address]] avnd::callback_storage<T> callbacks;

  [[no_unique_address]] avnd::smooth_storage<T> smooth;

  [[no_unique_address]] oscr::soundfile_storage<T> soundfiles;

  [[no_unique_address]] oscr::midifile_storage<T> midifiles;

#if defined(OSCR_HAS_MMAP_FILE_STORAGE)
  [[no_unique_address]] oscr::raw_file_storage<T> rawfiles;
#endif

  [[no_unique_address]] oscr::spectrum_storage<T> spectrums;

  [[no_unique_address]] controls_queue<T> control;

  using control_input_values_type
      = avnd::filter_and_apply<controls_type, avnd::control_input_introspection, T>;
  using control_output_values_type
      = avnd::filter_and_apply<controls_type, avnd::control_output_introspection, T>;
};

template <std::size_t Index, typename F>
constexpr void invoke_helper(F& f, auto& avnd_ports, auto& ossia_ports)
{
  f(avnd::pfr::get<Index>(avnd_ports), tuplet::get<Index>(ossia_ports),
    avnd::field_index<Index>{});
}

template <typename T, typename AudioCount>
class safe_node_base : public safe_node_base_base<T>
{
public:
  using processor_type = T;
  using inputs_t = typename avnd::inputs_type<T>::type;
  using outputs_t = typename avnd::outputs_type<T>::type;
  using param_in_info = avnd::parameter_input_introspection<T>;
  using midi_in_info = avnd::midi_input_introspection<T>;
  using midi_out_info = avnd::midi_output_introspection<T>;

  static constexpr int total_input_ports = avnd::total_input_count<T>();
  static constexpr int total_output_ports = avnd::total_output_count<T>();

  uint64_t instance{};

  safe_node_base(int buffer_size, double sample_rate, uint64_t uid)
      : safe_node_base_base<T>{buffer_size, sample_rate}
      , instance{uid}
  {
    this->buffer_size = buffer_size;
    this->sample_rate = sample_rate;

    this->m_inlets.reserve(total_input_ports + 1);
    this->m_outlets.reserve(total_output_ports + 1);

    this->audio_ports.init(this->m_inlets, this->m_outlets);
    this->message_ports.init(this->m_inlets);
    this->soundfiles.init(this->impl);

    if constexpr(avnd::sample_arg_processor<T> || avnd::sample_port_processor<T>)
    {
      this->smooth.init(this->impl, sample_rate);
    }
    else
    {
      // FIXME buffer_size can be variable: to be precise, we have to compute the ratio on each frame
      this->smooth.init(this->impl, sample_rate / buffer_size);
    }

    // constexpr const int total_input_channels = avnd::input_channels<T>(-1);
    // constexpr const int total_output_channels = avnd::output_channels<T>(-1);

    this->channels.set_input_channels(this->impl, 0, 2);
    this->channels.set_output_channels(this->impl, 0, 2);

    // TODO
    this->impl.init_channels(2, 2);
  }

  template <typename Functor>
  AVND_INLINE_FLATTEN void process_inputs_impl(Functor& f, auto& in)
  {
    using info = avnd::input_introspection<T>;
    [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
      using namespace tpl;
      (invoke_helper<Index>(f, in, this->ossia_inlets.ports), ...);
        }(typename info::indices_n{});
  }

  template <typename Functor>
  AVND_INLINE_FLATTEN void process_outputs_impl(Functor& f, auto& out)
  {
    using info = avnd::output_introspection<T>;
    [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
      (invoke_helper<Index>(f, out, this->ossia_outlets.ports), ...);
        }(typename info::indices_n{});
  }

  template <typename Functor, typename... Args>
  void process_all_ports(Args&&... args)
  {
    for(auto& [impl, i, o] : this->impl.full_state())
    {
      Functor f{*this, impl, args...};
      if constexpr(avnd::inputs_type<T>::size > 0)
        process_inputs_impl(f, i);
      if constexpr(avnd::outputs_type<T>::size > 0)
        process_outputs_impl(f, o);
    }
  }

  void initialize_all_ports()
  {
    // Setup inputs
    if constexpr(avnd::inputs_type<T>::size > 0)
    {
      using in_info = avnd::input_introspection<T>;
      using in_type = typename avnd::inputs_type<T>::type;
      auto& port_tuple = this->ossia_inlets.ports;

      [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        setup_inlets<safe_node_base_base<T>> init{*this, this->m_inlets};
        (init(
             avnd::field_reflection<Index, avnd::pfr::tuple_element_t<Index, in_type>>{},
             tuplet::get<Index>(port_tuple)),
         ...);
          }(typename in_info::indices_n{});
    }

    // Setup outputs
    if constexpr(avnd::outputs_type<T>::size > 0)
    {
      using out_info = avnd::output_introspection<T>;
      using out_type = typename avnd::outputs_type<T>::type;
      auto& port_tuple = this->ossia_outlets.ports;

      [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        setup_outlets<safe_node_base_base<T>> init{*this, this->m_outlets};
        (init(
             avnd::field_reflection<
                 Index, avnd::pfr::tuple_element_t<Index, out_type>>{},
             tuplet::get<Index>(port_tuple)),
         ...);
          }(typename out_info::indices_n{});
    }

    // Small setup step for the variable channel counts
    this->process_all_ports<setup_variable_audio_ports<safe_node_base, T>>();
  }

  void finish_init()
  {
    // Initialize the callbacks
    if constexpr(avnd::callback_introspection<outputs_t>::size > 0)
    {
      auto callbacks_initializer = [this]<
                                       typename Field, template <typename...> typename L,
                                       typename Ret, typename... Args, std::size_t Idx>(
                                       Field& field, L<Ret, Args...>, avnd::num<Idx>) {
        return do_callback<safe_node_base, Idx, Field, Ret, Args...>{*this, field};
      };
      this->callbacks.wrap_callbacks(this->impl, callbacks_initializer);
    }

    // Initialize the other ports
    this->initialize_all_ports();

    // Initialize the controls with their default values
    avnd::init_controls(this->impl);

    static_cast<AudioCount&>(*this).scan_audio_input_channels();
  }

  // Handling controls
  template <typename Val, std::size_t N>
  void control_updated_from_ui(Val&& new_value)
  {
    if constexpr(requires { avnd::effect_container<T>::multi_instance; })
    {
      for(const auto& state : this->impl.full_state())
      {
        // Replace the value in the field
        auto& field
            = avnd::control_input_introspection<T>::template get<N>(state.inputs);

        // OPTIMIZEME we're loosing a few allocations here that should be gc'd
        field.value = new_value;

        if_possible(field.update(state.effect));
      }
    }
    else
    {
      // Replace the value in the field
      auto& field
          = avnd::control_input_introspection<T>::template get<N>(this->impl.inputs());

      std::swap(field.value, new_value);

      if_possible(field.update(this->impl.effect));
    }

    // Mark the control as changed
    this->control.inputs_set.set(N);
  }

  void audio_configuration_changed()
  {
    // qDebug() << "New Audio configuration: "
    //          << this->channels.actual_runtime_inputs
    //          << this->channels.actual_runtime_outputs;
    // Allocate buffers, setup everything
    avnd::process_setup setup_info{
        .input_channels = this->channels.actual_runtime_inputs,
        .output_channels = this->channels.actual_runtime_outputs,
        .frames_per_buffer = this->buffer_size,
        .rate = this->sample_rate,
        .instance = this->instance};

    // This allocates the buffers that may be used for conversion
    // if e.g. we have an API that works with doubles,
    // and a plug-in that expects floats.
    // Here for instance we allocate buffers for an host that may invoke "process" with either floats or doubles.
    if constexpr(requires(AudioCount t) { t.processor; })
    {
      auto& self = static_cast<AudioCount&>(*this);
      self.processor.allocate_buffers(setup_info, float{});
      self.processor.allocate_buffers(setup_info, double{});
    }

    // Initialize the channels for the effect duplicator
    this->impl.init_channels(setup_info.input_channels, setup_info.output_channels);

    // Setup buffers for storing MIDI messages, FFTs, etc.

    {
      this->midi_buffers.reserve_space(this->impl, this->buffer_size);
      this->control_buffers.reserve_space(this->impl, this->buffer_size);
      this->spectrums.reserve_space(this->impl, this->buffer_size);
    }

    // Effect-specific preparation
    avnd::prepare(this->impl, setup_info);
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

  void update_tempo(double new_tempo)
  {
    using time_controls = avnd::time_control_input_introspection<T>;
    if constexpr(time_controls::size > 0)
    {
      this->tempo = new_tempo;
      time_controls::for_all_n2(
          this->impl.inputs(),
          [this, new_tempo](auto& field, auto pred_idx, auto f_idx) {
        this->time_controls.set_tempo(this->impl, pred_idx, f_idx, new_tempo);
          });
    }
  }

  bool prepare_run(const ossia::token_request& tk, int start, int frames)
  {
    // Update metadatas
    this->update_tempo(tk.tempo);

    // Check all the audio channels
    bool changed = static_cast<AudioCount&>(*this).scan_audio_input_channels();

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
    this->midi_buffers.clear_outputs(this->impl);

    // Clean up sample-accurate control output ports
    this->control_buffers.clear_outputs(this->impl);

    // Process inputs of all sorts
    this->process_all_ports<process_before_run<safe_node_base, T>>(start, frames);

    // Process messages
    if constexpr(avnd::messages_type<T>::size > 0)
      process_messages();

    return true;
  }

  template <auto Idx, typename M>
  void invoke_message(const ossia::value& val, avnd::field_reflection<Idx, M>)
  {
    if constexpr(!std::is_void_v<avnd::message_reflection<M>>)
    {
      using refl = avnd::message_reflection<M>;
      constexpr auto arg_count = refl::count;
      constexpr auto f = avnd::message_get_func<M>();

      if constexpr(arg_count == 0)
      {
        for(auto& m : this->impl.effects())
        {
          if constexpr(std::is_member_function_pointer_v<decltype(f)>)
          {
            if constexpr(requires { M{}(); })
              M{}();
            else
              (m.*f)();
          }
          else
            f();
        }
      }
      else if constexpr(arg_count == 1)
      {
        if constexpr(std::is_same_v<avnd::first_message_argument<M>, T&>)
        {
          for(auto& m : this->impl.effects())
          {
            if constexpr(std::is_member_function_pointer_v<decltype(f)>)
            {
              if constexpr(requires { M{}(m); })
                M{}(m);
              else if constexpr(requires { (m.*f)(m); })
                (m.*f)(m);
            }
            else
              f(m);
          }
        }
        else
        {
          constexpr M field;

          using arg_type = std::decay_t<avnd::first_message_argument<M>>;
          arg_type arg;
          from_ossia_value(field, val, arg, avnd::field_index<Idx>{});

          for(auto& m : this->impl.effects())
          {
            if constexpr(std::is_member_function_pointer_v<decltype(f)>)
            {
              if constexpr(requires { M{}(arg); })
                M{}(arg);
              else if constexpr(requires { (m.*f)(arg); })
                (m.*f)(arg);
            }
            else
            {
              f(arg);
            }
          }
          /*
          for (auto& m : this->impl.effects())
          {
            if constexpr (std::is_member_function_pointer_v<decltype(f)>)
            {
              if constexpr(requires { M{}(m); })
                M{}(m);
              else if constexpr(requires { (m.*f)(m); })
                (m.*f)(m);
            }
            else
              f(m);
          }*/
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
        if constexpr(arg_count == 2)
        {
          constexpr M field;
          using arg_type = std::decay_t<avnd::second_message_argument<M>>;
          arg_type arg;
          from_ossia_value(field, val, arg, avnd::field_index<Idx>{});
          for(auto& m : this->impl.effects())
          {
            if constexpr(std::is_member_function_pointer_v<decltype(f)>)
            {
              if constexpr(requires { M{}(m, arg); })
                M{}(m, arg);
              else if constexpr(requires { (m.*f)(m, arg); })
                (m.*f)(m, arg);
            }
            else
              f(m, arg);
          }
        }
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
    avnd::messages_introspection<T>::for_all([&](auto m) { process_message(m); });
  }

  void process_smooth() { this->smooth.smooth_all(this->impl); }

  auto make_controls_in_tuple()
  {
    // We only care about the inputs of the first one, since they're all the same
    for(auto& state : this->impl.full_state())
    {
      return avnd::control_input_introspection<T>::filter_tuple(
          state.inputs, [](auto& field) { return field.value; });
    }
  }
  auto make_controls_out_tuple()
  {
    // Note that this does not yet make a lot of sens for polyphonic effects
    for(auto& state : this->impl.full_state())
    {
      return avnd::control_output_introspection<T>::filter_tuple(
          state.outputs, [](auto& field) { return field.value; });
    }
  }

  void finish_run()
  {
    // Copy output events
    this->process_all_ports<process_after_run<safe_node_base, T>>();

    // Clean up MIDI inputs
    this->midi_buffers.clear_inputs(this->impl);

    // Clean up sample-accurate control input ports
    this->control_buffers.clear_inputs(this->impl);

    // Clear control bitsets for UI
    if constexpr(avnd::control_input_introspection<T>::size > 0)
    {
      if(this->control.inputs_set.any())
      {
        // Notify the UI
        this->control.ins_queue.enqueue(make_controls_in_tuple());
        this->control.inputs_set.reset();
      }
    }

    if constexpr(avnd::control_output_introspection<T>::size > 0)
    {
      if(this->control.outputs_set.any())
      {
        // Notify the UI
        this->control.outs_queue.enqueue(make_controls_out_tuple());
        this->control.outputs_set.reset();
      }
    }
  }

  std::string label() const noexcept override
  {
    return std::string{avnd::get_name<T>()};
  }

  template <typename Field, typename Val, std::size_t NField>
  inline constexpr bool from_ossia_value(
      Field& field, const ossia::value& src, Val& dst, avnd::field_index<NField>)
  {
    oscr::from_ossia_value(field, src, dst);
    return true;
  }

  template <avnd::smooth_parameter Field, typename Val, std::size_t NField>
  inline bool from_ossia_value(
      Field& field, const ossia::value& src, Val& dst, avnd::field_index<NField> idx)
  {
    Val next;
    oscr::from_ossia_value(src, next);
    this->smooth.update_target(field, next, idx);
    return false;
  }

  // Special case: this one may require the current tempo information
  template <avnd::time_control Field, typename Val, std::size_t NField>
  inline bool from_ossia_value(
      Field& field, const ossia::value& src, Val& dst, avnd::field_index<NField> idx)
  {
    auto vec = ossia::convert<ossia::vec2f>(src);
    if(vec[1] == 0.)
    {
      // Time in seconds, free mode
      dst = vec[0];
      this->time_controls.update_control(this->impl, idx, vec[0], false);
    }
    else
    {
      dst = to_seconds(vec[0], this->tempo);
      this->time_controls.update_control(this->impl, idx, vec[0], true);
    }
    return true;
  }

  void soundfile_release_request(std::string& str, int idx)
  {
    fprintf(stderr, "%s:%d\n", str.c_str(), idx);
  }

  void soundfile_load_request(std::string& str, int idx)
  {
    fprintf(stderr, "%s:%d\n", str.c_str(), idx);
  }
  void midifile_load_request(std::string& str, int idx)
  {
    fprintf(stderr, "%s:%d\n", str.c_str(), idx);
  }
  void raw_file_load_request(std::string& str, int idx)
  {
    fprintf(stderr, "%s:%d\n", str.c_str(), idx);
  }

  template <std::size_t N, std::size_t NField>
  void soundfile_loaded(
      ossia::audio_handle& hdl, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    this->soundfiles.load(
        this->impl, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
  }
  template <std::size_t N, std::size_t NField>
  void midifile_loaded(
      const std::shared_ptr<oscr::midifile_data>& hdl, avnd::predicate_index<N>,
      avnd::field_index<NField>)
  {
    this->midifiles.load(
        this->impl, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
  }
#if OSCR_HAS_MMAP_FILE_STORAGE
  template <std::size_t N, std::size_t NField>
  void file_loaded(
      const std::shared_ptr<oscr::raw_file_data>& hdl, avnd::predicate_index<N>,
      avnd::field_index<NField>)
  {
    this->rawfiles.load(
        this->impl, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
  }
#endif
};

// FIXME these concepts are super messy

template <typename FP, typename T>
concept real_mono_processor = avnd::mono_per_sample_arg_processor<FP, T>
                              || avnd::mono_per_sample_port_processor<FP, T>
                              || avnd::monophonic_single_port_audio_effect<FP, T>
                              || avnd::mono_per_channel_arg_processor<FP, T>
                              || avnd::mono_per_channel_port_processor<FP, T>;
template <typename T>
concept real_good_mono_processor
    = real_mono_processor<float, T> || real_mono_processor<double, T>;

template <typename T>
concept ossia_compatible_nonaudio_processor
    = !(avnd::audio_argument_processor<T> || avnd::audio_port_processor<T>);

template <typename T>
concept ossia_compatible_audio_processor
    = avnd::poly_sample_array_input_port_count<double, T> > 0
      || avnd::poly_sample_array_output_port_count<double, T> > 0;

template <typename T>
class safe_node;

struct tick_info
{
  const ossia::token_request& tk;
  ossia::exec_state_facade st;
  int64_t m_frames{};

  int64_t frames() const noexcept { return m_frames; }

  int64_t position_in_frames() const noexcept
  {
    return tk.start_date_to_physical(st.modelToSamples());
  }

  auto signature() const noexcept
  {
    struct sig
    {
      int num{}, denom{};
    };
    return sig{.num = tk.signature.upper, .denom = tk.signature.lower};
  }
  double speed() const noexcept { return tk.speed; };
  double tempo() const noexcept { return tk.tempo; };
  double relative_position() const noexcept { return tk.position(); };
  double parent_duration() const noexcept { return tk.parent_duration.impl; };
  double position_in_seconds() const noexcept
  {
    return position_in_nanoseconds() / 1e9;
  };
  double position_in_nanoseconds() const noexcept
  {
    return (st.currentDate() - st.startDate());
  };
  double start_position_in_quarters() const noexcept
  {
    return tk.musical_start_position;
  };
  double end_position_in_quarters() const noexcept { return tk.musical_end_position; };
  double bar_at_start() const noexcept { return tk.musical_start_last_bar; };
  double bar_at_end() const noexcept { return tk.musical_end_last_bar; };
};
}
