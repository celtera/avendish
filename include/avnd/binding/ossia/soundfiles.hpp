#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/all.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <ossia/dataflow/nodes/media.hpp>
#include <boost/mp11.hpp>

namespace oscr
{
template<typename T>
using channel_vector = ossia::small_vector<T, 8>;
template<typename T>
struct audio_data_t
{
  ossia::pod_vector<T> data;
  std::string path;
};

// Field: struct { struct { float** data; } soundfile; }
// This gives us: float*
template <typename Field>
using soundfile_channel_type
    = std::remove_pointer_t<std::remove_reference_t<decltype(Field{}.soundfile.data)>>;

template <typename Field>
struct soundfile_handle_type;

template <typename Field>
requires std::is_convertible_v<soundfile_channel_type<Field>, const float*>
struct soundfile_handle_type<Field> : ossia::audio_handle { };

template <typename Field>
requires std::is_convertible_v<soundfile_channel_type<Field>, const double*>
struct soundfile_handle_type<Field> : audio_data_t<double> { };

template <typename T>
struct soundfile_input_storage
{
};

template <typename T>
requires(avnd::soundfile_input_introspection<T>::size > 0)
struct soundfile_input_storage<T>
{
  // std::tuple< float*, double* >
  using ptr_tuple = avnd::filter_and_apply<
    soundfile_channel_type,
    avnd::soundfile_input_introspection,
    T>;
  using hdl_tuple = avnd::filter_and_apply<
    soundfile_handle_type,
    avnd::soundfile_input_introspection,
    T>;

  using ptr_vectors = boost::mp11::mp_transform<channel_vector, ptr_tuple>;

  // std::tuple< std::vector<float*>, std::vector<double*> >
  [[no_unique_address]] ptr_vectors pointers;

  // std::tuple< ossia::audio_handle, ossia::audio_handle >
  [[no_unique_address]] hdl_tuple handles;
};


/**
 * Used to store RAM-loaded soundfiles channel pointers
 */
template <typename T>
struct soundfile_storage
    : soundfile_input_storage<T>
{
  using sf_in = avnd::soundfile_input_introspection<T>;

  void init(avnd::effect_container<T>& t)
  {
    if constexpr (sf_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M& port, avnd::predicate_index<Idx>)
      {
        // Get the matching buffer in our storage, a std::vector<timed_value>
        auto& buf = get<Idx>(this->pointers);

        // Preallocate some space for 2 channels
        buf.reserve(2);

        // Assign the pointer to the std::span<timed_value> values; member in the port
        port.soundfile.data = nullptr;
        port.soundfile.frames = 0;
        port.soundfile.channels = 0;
        port.soundfile.filename = "";
      };
      sf_in::for_all_n(avnd::get_inputs(t), init_raw_in);
    }
  }

  template<std::size_t N, std::size_t NField>
  void load(avnd::effect_container<T>& t, ossia::audio_handle& hdl, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    auto& buf = get<N>(this->pointers);
    using pointer_type = typename std::decay_t<decltype(buf)>::value_type;
    const int chans = hdl->data.size();
    const int64_t frames = chans > 0 ? hdl->data[0].size() : 0;

    if constexpr(std::is_same_v<pointer_type, const ossia::audio_sample*>)
    {
      ossia::audio_handle& g = get<N>(this->handles);

      // Store the handle to keep the memory from being freed
      std::exchange(g, hdl);
      buf.resize(chans);

      // Copy the pointers in our storage if no conversion is needed
      for(int i = 0; i < chans; i++)
       buf[i] = g->data[i].data();

      // Update the port
      auto& port = avnd::pfr::get<NField>(t.inputs());
      port.soundfile.data = buf.data();
      port.soundfile.frames = frames;
      port.soundfile.channels = chans;
      port.soundfile.filename = g->path;
    }
    else
    {
      audio_data_t<double>& g = get<N>(this->handles);

      // FIXME this allocates. :(
      g.data.reserve(chans * frames);
      buf.resize(chans);
      g.path = hdl->path;

      // Copy to the double storage
      for(int i = 0; i < chans; i++)
      {
        g.data.insert(g.data.end(), hdl->data[i].begin(), hdl->data[i].end());
        buf[i] = g.data.data() + i * frames;
      }

      // Update the port
      auto& port = avnd::pfr::get<NField>(t.inputs());
      port.soundfile.data = buf.data();
      port.soundfile.frames = frames;
      port.soundfile.channels = chans;
      port.soundfile.filename = g.path;
    }
  }
};

}
