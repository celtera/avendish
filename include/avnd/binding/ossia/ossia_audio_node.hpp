#pragma once
#include <avnd/binding/ossia/node.hpp>
namespace oscr
{

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
    auto tm = st.timings(tk);
    auto start = tm.start_sample;
    auto frames = tm.length;

    // Compute channel count
    int total_input_channels = 0;
    in_busses::for_all_n2(this->impl.inputs(),
                          [&] <std::size_t NPred, std::size_t NField> (auto& field, avnd::predicate_index<NPred> pred_idx, avnd::field_index<NField> f_idx) {
      ossia::audio_inlet& port = tuplet::get<NField>(this->ossia_inlets.ports);

      if constexpr(requires { field.channels(); })
        port->set_channels(field.channels());

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
        cur_ptr[i] = port->channel(i).data() + start;
      }

      if_possible(field.samples = cur_ptr)
      else if_possible(field.samples = (const double**)cur_ptr)
      else static_assert(field.samples.wrong_type);

      if_possible(field.channels = port->channels());

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
        cur_ptr[i] = port->channel(i).data() + start;
      }

      field.samples = cur_ptr;

      out_ptr += chans;
    });


    avnd::invoke_effect(this->impl, tick_info {tk, st, frames});

    this->finish_run();
  }
};

}
