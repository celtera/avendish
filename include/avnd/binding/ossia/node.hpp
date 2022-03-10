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
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
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
|| avnd::mono_per_sample_arg_processor<float, T>
|| avnd::monophonic_arg_audio_effect<float, T>
|| avnd::polyphonic_arg_audio_effect<float, T>
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

template<typename Field>
using controls_type = std::decay_t<decltype(Field::value)>;

template<typename T>
using atomic_shared_ptr = boost::atomic_shared_ptr<T>;

template<typename T>
struct controls_mirror
{
  static constexpr int i_size = avnd::control_input_introspection<T>::size;
  static constexpr int o_size = avnd::control_output_introspection<T>::size;
  using i_tuple = avnd::filter_and_apply<controls_type, avnd::control_input_introspection, T>;
  using o_tuple = avnd::filter_and_apply<controls_type, avnd::control_output_introspection, T>;

  controls_mirror()
  {
    inputs.load(new i_tuple);
    outputs.load(new o_tuple);
  }

  [[no_unique_address]]
  atomic_shared_ptr<i_tuple> inputs;
  [[no_unique_address]]
  atomic_shared_ptr<o_tuple> outputs;

  std::bitset<i_size> inputs_bits;
  std::bitset<o_size> outputs_bits;
};

template<typename T>
struct controls_queue
{
  static constexpr int i_size = avnd::control_input_introspection<T>::size;
  static constexpr int o_size = avnd::control_output_introspection<T>::size;
  using i_tuple = avnd::filter_and_apply<controls_type, avnd::control_input_introspection, T>;
  using o_tuple = avnd::filter_and_apply<controls_type, avnd::control_output_introspection, T>;

  moodycamel::ConcurrentQueue<i_tuple> ins_queue;
  moodycamel::ConcurrentQueue<o_tuple> outs_queue;

  std::bitset<i_size> inputs_set;
  std::bitset<o_size> outputs_set;
};
template <typename T>
class safe_node_base_base
        : public ossia::nonowning_graph_node
{
public:
  safe_node_base_base(int buffer_size, double sample_rate)
    : channels{this->impl}
  {

  }

  int buffer_size{};
  double sample_rate{};

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

  // [[no_unique_address]]
  // controls_mirror<T> feedback;

  [[no_unique_address]]
  controls_queue<T> control;

  using control_input_values_type = avnd::filter_and_apply<controls_type, avnd::control_input_introspection, T>;
  using control_output_values_type = avnd::filter_and_apply<controls_type, avnd::control_output_introspection, T>;

};


template <typename T, typename AudioCount>
class safe_node_base
        : public safe_node_base_base<T>
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

  safe_node_base(int buffer_size, double sample_rate)
      : safe_node_base_base<T>{buffer_size, sample_rate}
  {
    this->buffer_size = buffer_size;
    this->sample_rate = sample_rate;

    this->m_inlets.reserve(total_input_ports + 1);
    this->m_outlets.reserve(total_output_ports + 1);

    this->audio_ports.init(this->m_inlets, this->m_outlets);
    this->message_ports.init(this->m_inlets);

    // constexpr const int total_input_channels = avnd::input_channels<T>(-1);
    // constexpr const int total_output_channels = avnd::output_channels<T>(-1);

    this->channels.set_input_channels(this->impl, 0, 2);
    this->channels.set_output_channels(this->impl, 0, 2);

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
    // Clear the current state of changed controls
      /*
      moodycamel::ConcurrentQueue<i_tuple> ins_queue;
      moodycamel::ConcurrentQueue<o_tuple> outs_queue;

      i_tuple last_inputs;
      i_tuple last_outputs;

      std::bitset<i_size> inputs_set;
      std::bitset<o_size> outputs_set;
      */
    if constexpr (avnd::inputs_type<T>::size > 0)
      process_inputs(f, this->impl.inputs());
    if constexpr (avnd::outputs_type<T>::size > 0)
      process_outputs(f, this->impl.outputs());
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
          setup_inlets<safe_node_base_base<T>> init{*this, this->m_inlets};
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
          setup_outlets<safe_node_base_base<T>> init{*this, this->m_outlets};
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
      this->callbacks.wrap_callbacks(this->impl, callbacks_initializer);
    }

    // Initialize the other ports
    this->initialize_all_ports();

    this->audio_configuration_changed();
  }


  // Handling controls
  template<typename Val, std::size_t N>
  void control_updated_from_ui(Val&& new_value)
  {
    // Replace the value in the field
    auto& field = avnd::control_input_introspection<T>::template get<N>(
                avnd::get_inputs<T>(this->impl)
    );

    std::swap(field.value, new_value);

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
        .rate = this->sample_rate};

    // This allocates the buffers that may be used for conversion
    // if e.g. we have an API that works with doubles,
    // and a plug-in that expects floats.
    // Here for instance we allocate buffers for an host that may invoke "process" with either floats or doubles.
    this->processor.allocate_buffers(setup_info, float{});
    this->processor.allocate_buffers(setup_info, double{});

    // Initialize the channels for the effect duplicator
    this->impl.init_channels(setup_info.input_channels, setup_info.output_channels);

    // Setup buffers for storing MIDI messages

    {
     this->midi_buffers.reserve_space(this->impl, this->buffer_size);
    }

    {
      this->control_buffers.reserve_space(this->impl, this->buffer_size);
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


  bool prepare_run(int start, int frames)
  {
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

  auto make_controls_in_tuple()
  {
    return avnd::control_input_introspection<T>::filter_tuple(
                avnd::get_inputs<T>(this->impl),
                [] (auto& field) { return field.value; });
  }
  auto make_controls_out_tuple()
  {
    return avnd::control_output_introspection<T>::filter_tuple(
                avnd::get_outputs<T>(this->impl),
                [] (auto& field) { return field.value; });
  }

  void finish_run()
  {
     // Copy output events
     process_all_ports(process_after_run<safe_node_base>{*this});

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
};

// FIXME these concepts are super messy

template<typename FP, typename T>
concept real_mono_processor =
        avnd::mono_per_sample_arg_processor<FP, T>
     || avnd::mono_per_sample_port_processor<FP, T>
     || avnd::monophonic_arg_audio_effect<FP, T>
     || avnd::monophonic_single_port_audio_effect<FP, T>
     || avnd::mono_per_channel_arg_processor<FP, T>
     || avnd::mono_per_channel_port_processor<FP, T>;
template<typename T>
concept real_good_mono_processor = real_mono_processor<float, T> || real_mono_processor<double, T>;

template<typename T>
class safe_node;


}
