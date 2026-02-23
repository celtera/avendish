#pragma once
#include <avnd/binding/ossia/node.hpp>
namespace oscr
{
#if 0
template <typename Info, typename Type>
std::span<Type*> avnd_port_idx_to_ossia_ports_old(
    oscr::dynamic_ports_storage<Info>& dynamic_ports,
    ossia::small_vector<Type, 2>& ossia_ports, int index) noexcept
{
  int model_index = 0;

  // We have to adjust before accessing a port as there is the first "fake"
  // port if the processor takes audio by argument
  if constexpr(avnd::audio_argument_processor<Info>)
    model_index += 1;
  else if constexpr(avnd::tag_cv<Info>)
    model_index += 1;

  // The "messages" ports also go before
  model_index += avnd::messages_introspection<Info>::size;

  std::span<ossia::inlet*> ret;
  if constexpr(avnd::dynamic_ports_input_introspection<Info>::size == 0)
  {
    ret = std::span<Type>(
        const_cast<Type*>(ossia_ports.data()) + model_index + index, 1);
  }
  else
  {
    avnd::input_introspection<Info>::for_all(
        [&dynamic_ports, index, &model_index, &ossia_ports,
         &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
      if(Idx == index)
      {
        int num_ports = 1;
        if constexpr(avnd::dynamic_ports_port<P>)
        {
          num_ports = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
          if(num_ports == 0)
          {
            ret = {};
            return;
          }
        }
        ret = std::span<Type>(
            const_cast<Type*>(ossia_ports.data()) + model_index, num_ports);
      }
      else
      {
        if constexpr(avnd::dynamic_ports_port<P>)
        {
          model_index += dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
        }
        else
        {
          model_index += 1;
        }
      }
    });
  }

  return ret;
}
template <typename Info, typename Type, std::size_t N>
std::span<Type*> avnd_port_idx_to_ossia_ports(
    oscr::dynamic_ports_storage<Info>& dynamic_ports,
    oscr::outlet_storage<Info>& ossia_ports) noexcept
{
  int model_index = 0;
  
  // We have to adjust before accessing a port as there is the first "fake"
  // port if the processor takes audio by argument
  if constexpr(avnd::audio_argument_processor<Info>)
    model_index += 1;
  else if constexpr(avnd::tag_cv<Info>)
    model_index += 1;
  
  // The "messages" ports also go before
  model_index += avnd::messages_introspection<Info>::size;
  
  std::span<ossia::inlet*> ret;
  if constexpr(avnd::dynamic_ports_input_introspection<Info>::size == 0)
  {
    ret = std::span<Type>(
        const_cast<Type*>(ossia_ports.data()) + model_index + index, 1);
  }
  else
  {
    avnd::input_introspection<Info>::for_all(
        [&dynamic_ports, index, &model_index, &ossia_ports,
         &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
      if(Idx == index)
      {
        int num_ports = 1;
        if constexpr(avnd::dynamic_ports_port<P>)
        {
          num_ports = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
          if(num_ports == 0)
          {
            ret = {};
            return;
          }
        }
        ret = std::span<Type>(
            const_cast<Type*>(ossia_ports.data()) + model_index, num_ports);
      }
      else
      {
        if constexpr(avnd::dynamic_ports_port<P>)
        {
          model_index += dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
        }
        else
        {
          model_index += 1;
        }
      }
    });
  }
  
  return ret;
}
#endif
// Special case for the case which matches exactly the ossia situation
template <ossia_compatible_dynamic_audio_processor T>
class safe_node<T> : public safe_node_base<T, safe_node<T>>
{
public:
  using in_channels = avnd::dynamic_mono_audio_port_input_introspection<T>;
  using out_channels = avnd::dynamic_mono_audio_port_output_introspection<T>;
  using in_busses = avnd::dynamic_poly_audio_port_input_introspection<T>;
  using out_busses = avnd::dynamic_poly_audio_port_output_introspection<T>;
  using safe_node_base<T, safe_node<T>>::safe_node_base;

  static inline std::vector<double> zeros_in, zeros_out;

  template <typename... Args>
  safe_node(Args&&... args)
      : safe_node_base<T, safe_node<T>>{std::forward<Args>(args)...}
  {
    zeros_in.resize(4096);
    zeros_out.resize(4096);
  }

  bool scan_audio_input_channels() { return false; }

  template <std::size_t NPred>
  int compute_output_channels(auto& field)
  {
    int actual_channels = 0;
    if constexpr(requires { field.ports; })
    {
      // avnd::dynamic_port
      for(auto& port : field.ports)
      {
        // channel matching logic explicitly disabled here:
        actual_channels += compute_output_channels<-1>(port);
      }
    }
    else if constexpr(requires { field.channels(); })
    {
      // Fixed at compile-time
      actual_channels = field.channels();
    }
    else if constexpr(requires { field.request_channels; })
    {
      // Explicitly requested: the channels are already set in field.channels;
      actual_channels = field.channels;
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
        auto& mimicked_port = in_busses::template field<0>(inputs);
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
    auto tm = st.timings(tk);
    auto start = tm.start_sample;
    auto frames = tm.length;

    // Compute input channel count (from busses)
    int total_input_channels = 0;
    in_busses::for_all_n2(
        this->impl.inputs(), [&]<std::size_t NPred, std::size_t NField>(
                                 auto& field, avnd::predicate_index<NPred> pred_idx,
                                 avnd::field_index<NField> f_idx) {
      ossia::audio_inlet& port = tuplet::get<NField>(this->ossia_inlets.ports);

      if constexpr(requires { field.channels(); })
        port->set_channels(field.channels());

      total_input_channels += port->channels();
    });

    // Compute input channel count (from mono channels / dynamic ports)
    in_channels::for_all_n2(
        this->impl.inputs(), [&]<typename Field, std::size_t NPred, std::size_t NField>(
                                 Field& field, avnd::predicate_index<NPred> pred_idx,
                                 avnd::field_index<NField> f_idx) {
      if constexpr(avnd::dynamic_ports_port<Field>)
      {
        const auto& ports = tuplet::get<NField>(this->ossia_inlets.ports);
        int port_min = std::min(field.ports.size(), ports.size());
        for(int port_index = 0; port_index < port_min; port_index++)
        {
          ossia::inlet* out = ports[port_index];
          ossia::audio_port& ossia_port = out->cast<ossia::audio_port>();
          // FIXME in the future maybe we want multiple dynamic_ports ?
          ossia_port.set_channels(1);
          ossia_port.channel(0).resize(st.bufferSize());

          field.ports[port_index].channel = ossia_port.channel(0).data() + start;
          total_input_channels++;
        }
      }
      else
      {
        ossia::audio_inlet& port = tuplet::get<NField>(this->ossia_outlets.ports);
        port->set_channels(1);

        port->channel(0).resize(st.bufferSize());
        field.channel = port->channel(0).data() + start;
        total_input_channels++;
      }
    });

    // 1. Set & compute the expected channels
    int expected_output_channels = 0;
    out_busses::for_all_n2(
        this->impl.outputs(), [&]<std::size_t NPred, std::size_t NField>(
                                  auto& field, avnd::predicate_index<NPred> pred_idx,
                                  avnd::field_index<NField> f_idx) {
      expected_output_channels += this->template compute_output_channels<NPred>(field);
    });

    // 2. Apply prepare
    if(!this->prepare_run(tk, st, start, frames))
    {
      this->finish_run();
      return;
    }

    // 3. Check the number of channels we aactually got and set the pointers

    double** in_ptr = (double**)alloca(sizeof(double*) * (1 + total_input_channels));
    // Set the pointers in the input ports: for busses
    in_busses::for_all_n2(
        this->impl.inputs(), [&]<typename Field, std::size_t NPred, std::size_t NField>(
                                 Field& field, avnd::predicate_index<NPred> pred_idx,
                                 avnd::field_index<NField> f_idx) {
      if constexpr(avnd::dynamic_ports_port<Field>)
      {
        // TODO
      }
      else
      {
        ossia::audio_inlet& port = get<NField>(this->ossia_inlets.ports);

        auto cur_ptr = in_ptr;

        for(int i = 0; i < port->channels(); i++)
        {
          port->channel(i).resize(st.bufferSize());
          cur_ptr[i] = port->channel(i).data() + start;
        }

        // clang-format off
        if_possible(field.samples = cur_ptr)
            else if_possible(field.samples = (const double**)cur_ptr)
            else static_assert(field.samples.wrong_type);
        // clang-format on

        if_possible(field.channels = port->channels());

        in_ptr += port->channels();
      }
    });

    // For channels:

    in_channels::for_all_n2(
        this->impl.inputs(), [&]<typename Field, std::size_t NPred, std::size_t NField>(
                                 Field& field, avnd::predicate_index<NPred> pred_idx,
                                 avnd::field_index<NField> f_idx) {
      if constexpr(avnd::dynamic_ports_port<Field>)
      {
        const auto& ports = tuplet::get<NField>(this->ossia_inlets.ports);
        int port_min = std::min(field.ports.size(), ports.size());
        for(int port_index = 0; port_index < port_min; port_index++)
        {
          ossia::inlet* out = ports[port_index];
          ossia::audio_port& port = out->cast<ossia::audio_port>();
          // FIXME in the future maybe we want multiple dynamic_ports ?
          port.set_channels(1);
          port.channel(0).resize(st.bufferSize());

          field.ports[port_index].channel = port.channel(0).data() + start;
        }
        for(int port_index = port_min; port_index < field.ports.size(); port_index++)
        {
          field.ports[port_index].channel = this->zeros_in.data();
        }
      }
      else
      {
        ossia::audio_inlet& port = tuplet::get<NField>(this->ossia_outlets.ports);
        port->set_channels(1);

        port->channel(0).resize(st.bufferSize());
        field.channel = port->channel(0).data() + start;
      }
    });

    int output_channels = 0;
    out_busses::for_all_n2(
        this->impl.outputs(), [&]<std::size_t NPred, std::size_t NField>(
                                  auto& field, avnd::predicate_index<NPred> pred_idx,
                                  avnd::field_index<NField> f_idx) {
      output_channels += this->template compute_output_channels<NPred>(field);
    });

    // Smooth
    this->process_smooth();

    out_channels::for_all_n2(
        this->impl.outputs(), [&]<typename Field, std::size_t NPred, std::size_t NField>(
                                  Field& field, avnd::predicate_index<NPred> pred_idx,
                                  avnd::field_index<NField> f_idx) {
      if constexpr(avnd::dynamic_ports_port<Field>)
      {
        const auto& ports = tuplet::get<NField>(this->ossia_outlets.ports);
        int port_min = std::min(field.ports.size(), ports.size());
        for(int port_index = 0; port_index < port_min; port_index++)
        {
          ossia::outlet* out = ports[port_index];
          ossia::audio_port& ossia_port = out->cast<ossia::audio_port>();
          // FIXME in the future maybe we want multiple dynamic_ports ?
          ossia_port.set_channels(1);
          ossia_port.channel(0).resize(st.bufferSize());

          field.ports[port_index].channel = ossia_port.channel(0).data() + start;
        }
        for(int port_index = port_min; port_index < field.ports.size(); port_index++)
        {
          field.ports[port_index].channel = this->zeros_out.data();
        }
      }
      else
      {
        ossia::audio_outlet& port = tuplet::get<NField>(this->ossia_outlets.ports);
        port->set_channels(1);

        port->channel(0).resize(st.bufferSize());
        field.channel = port->channel(0).data() + start;
      }
    });

    double** out_ptr = (double**)alloca(sizeof(double*) * (1 + output_channels));

    out_busses::for_all_n2(
        this->impl.outputs(), [&]<typename Field, std::size_t NPred, std::size_t NField>(
                                  Field& field, avnd::predicate_index<NPred> pred_idx,
                                  avnd::field_index<NField> f_idx) {
      const int chans = this->template compute_output_channels<NPred>(field);
      if constexpr(avnd::dynamic_ports_port<Field>)
      {
        const auto& ports = tuplet::get<NField>(this->ossia_outlets.ports);
        int port_min = std::min(field.ports.size(), ports.size());
        for(int port_index = 0; port_index < port_min; port_index++)
        {
          ossia::outlet* out = ports[port_index];
          ossia::audio_port& port = out->cast<ossia::audio_port>();
          // FIXME in the future maybe we want multiple dynamic_ports ?
          port.set_channels(1);

          port.channel(0).resize(st.bufferSize());
          out_ptr[0] = port.channel(0).data() + start;

          field.ports[port_index].samples = out_ptr;
          field.ports[port_index].channels = 1;
          out_ptr += 1;
        }
      }
      else
      {
        ossia::audio_outlet& port = tuplet::get<NField>(this->ossia_outlets.ports);
        port->set_channels(chans);

        auto cur_ptr = out_ptr;

        for(int i = 0; i < chans; i++)
        {
          port->channel(i).resize(st.bufferSize());
          cur_ptr[i] = port->channel(i).data() + start;
        }

        field.samples = cur_ptr;

        out_ptr += chans;
      }
    });

    avnd::invoke_effect(
        this->impl,
        avnd::get_tick_or_frames(this->impl, tick_info{*this, tk, st, frames}));

    this->finish_run();
  }
};

}
