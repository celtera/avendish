#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/all.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <boost/mp11.hpp>
#include <ossia/dataflow/nodes/media.hpp>

namespace oscr
{
template <typename T>
using channel_vector = ossia::small_vector<T, 8>;
template <typename T>
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
struct soundfile_handle_type<Field> : ossia::audio_handle
{
};

template <typename Field>
  requires std::is_convertible_v<soundfile_channel_type<Field>, const double*>
struct soundfile_handle_type<Field> : audio_data_t<double>
{
};

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
      soundfile_channel_type, avnd::soundfile_input_introspection, T>;
  using hdl_tuple = avnd::filter_and_apply<
      soundfile_handle_type, avnd::soundfile_input_introspection, T>;

  using ptr_vectors = boost::mp11::mp_transform<channel_vector, ptr_tuple>;

  // std::tuple< std::vector<float*>, std::vector<double*> >
  AVND_NO_UNIQUE_ADDRESS ptr_vectors pointers;

  // std::tuple< ossia::audio_handle, ossia::audio_handle >
  AVND_NO_UNIQUE_ADDRESS hdl_tuple handles;
};

/**
 * Used to store RAM-loaded soundfiles channel pointers
 */
template <typename T>
struct soundfile_storage : soundfile_input_storage<T>
{
  using sf_in = avnd::soundfile_input_introspection<T>;

  void init(avnd::effect_container<T>& t)
  {
    if constexpr(sf_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
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

  //! Returns the handle that the caller is now responsible for disposing of,
  //! in a thread where deallocating is allowed.
  template <std::size_t N, std::size_t NField>
  ossia::audio_handle load(
      avnd::effect_container<T>& t, ossia::audio_handle& hdl, avnd::predicate_index<N>,
      avnd::field_index<NField>)
  {
    auto& buf = get<N>(this->pointers);
    using pointer_type = typename std::decay_t<decltype(buf)>::value_type;
    const int chans = hdl->data.size();
    const int rate = hdl->rate;
    const int64_t frames = chans > 0 ? hdl->data[0].size() : 0;

    if constexpr(std::is_same_v<pointer_type, const ossia::audio_sample*>)
    {
      ossia::audio_handle& g = get<N>(this->handles);

      // Store the handle to keep the memory from being freed.
      // The previous one is given back to the caller: the ports still point
      // into it until they are updated below, and freeing it here would happen
      // in whichever thread called us - generally the audio thread.
      ossia::audio_handle previous = std::exchange(g, hdl);
      buf.resize(chans);

      // Copy the pointers in our storage if no conversion is needed
      for(int i = 0; i < chans; i++)
        buf[i] = g->data[i].data();

      // Update the port
      for(auto state : t.full_state())
      {
        auto& port = avnd::pfr::get<NField>(state.inputs);
        port.soundfile.data = buf.data();
        port.soundfile.frames = frames;
        port.soundfile.channels = chans;
        port.soundfile.rate = rate;
        port.soundfile.filename = g->path;

        if_possible(port.update(state.effect));
      }
      return previous;
    }
    else
    {
      audio_data_t<double>& g = get<N>(this->handles);

      // FIXME this allocates. :(
      g.data.clear();
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
      for(auto state : t.full_state())
      {
        auto& port = avnd::pfr::get<NField>(state.inputs);
        port.soundfile.data = buf.data();
        port.soundfile.frames = frames;
        port.soundfile.channels = chans;
        port.soundfile.rate = rate;
        port.soundfile.filename = g.path;

        if_possible(port.update(state.effect));
      }

      // The samples were copied into our own storage: nothing keeps hdl alive
      return hdl;
    }
  }

  // std::function<void(std::string& str, int idx)> release_request;
  std::function<void(std::string& str, int idx)> load_request;
};
}

#include <libremidi/reader.hpp>
namespace oscr
{
struct midifile_data
{
  libremidi::reader reader;
  std::string filename;
};

template <typename Field>
struct midifile_handle_type;

template <avnd::midifile_port Field>
struct midifile_handle_type<Field> : std::shared_ptr<midifile_data>
{
};

template <typename T>
struct midifile_input_storage
{
};

template <typename T>
  requires(avnd::midifile_input_introspection<T>::size > 0)
struct midifile_input_storage<T>
{
  using hdl_tuple = avnd::filter_and_apply<
      midifile_handle_type, avnd::midifile_input_introspection, T>;

  // std::tuple< std::shared_ptr<midifile_data> >
  AVND_NO_UNIQUE_ADDRESS hdl_tuple handles;
};

template <typename T>
struct midifile_storage : midifile_input_storage<T>
{
  void init(avnd::effect_container<T>& t) { }

  //! \see soundfile_storage::load
  template <std::size_t N, std::size_t NField>
  std::shared_ptr<midifile_data> load(
      avnd::effect_container<T>& t, const std::shared_ptr<midifile_data>& hdl,
      avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    std::shared_ptr<midifile_data>& g = get<N>(this->handles);

    // Store the handle to keep the memory from being freed.
    // The previous one is given back to the caller, see soundfile_storage::load
    std::shared_ptr<midifile_data> previous = std::exchange(g, hdl);

    for(auto state : t.full_state())
    {
      avnd::midifile_port auto& port = avnd::pfr::get<NField>(state.inputs);

      auto& tracks = hdl->reader.tracks;
      port.midifile.tracks.clear();
      port.midifile.tracks.resize(tracks.size());

      int64_t max_length = 0;
      for(std::size_t i = 0; i < tracks.size(); i++)
      {
        auto& in = tracks[i];
        auto& out = port.midifile.tracks[i];
        using message_type = std::decay_t<decltype(out[0])>;
        using bytes_type = std::remove_reference_t<decltype(message_type::bytes)>;
        static constexpr bool is_c_array = std::is_bounded_array_v<bytes_type>;

        if constexpr(is_c_array)
          out.reserve(in.size());
        else
          out.resize(in.size());

        int64_t abs_tick = 0;
        auto set_tick = [&abs_tick](auto& in, auto& out) mutable {
          // Compute the tick
          auto delta_tick = in.tick;
          abs_tick += delta_tick;
          if constexpr(requires { out.tick_delta; })
            out.tick_delta = delta_tick;
          if constexpr(requires { out.tick_absolute; })
            out.tick_absolute = abs_tick;
        };

        for(std::size_t k = 0; k < in.size(); ++k)
        {
          // Copy the MIDI bytes
          auto& in_b = in[k].m.bytes;
          if constexpr(is_c_array)
          {
            static_assert(
                std::extent<bytes_type, 0>::value == 3,
                "MIDI arrays must have at least 3 bytes");
            if(in_b.size() != 3)
              continue;

            message_type m{.bytes{in_b[0], in_b[1], in_b[2]}};
            set_tick(in[k], m);
            out.push_back(std::move(m));
          }
          else
          {
            out[k].bytes.assign(std::begin(in_b), std::end(in_b));

            set_tick(in[k], out[k]);
          }
        }

        if(abs_tick > max_length)
          max_length = abs_tick;
      }

      port.midifile.filename = hdl->filename;

      if constexpr(requires { port.midifile.ticks_per_beat; })
        port.midifile.ticks_per_beat = hdl->reader.ticksPerBeat;
      if constexpr(requires { port.midifile.starting_tempo; })
        port.midifile.starting_tempo = hdl->reader.startingTempo;
      if constexpr(requires { port.midifile.length; })
        port.midifile.length = max_length;

      if_possible(port.update(state.effect));
    }
    return previous;
  }
};
}

#if __has_include(<QFile>)
#define OSCR_HAS_MMAP_FILE_STORAGE 1
#include <QFile>
#include <vector>
namespace oscr
{
struct raw_file_data
{
  QFile file;
  QByteArray data;
  std::string filename;
};

template <typename Field>
struct raw_file_handle_type;

template <avnd::raw_file_port Field>
struct raw_file_handle_type<Field> : std::shared_ptr<raw_file_data>
{
};

template <typename T>
struct raw_file_input_storage
{
};

template <typename T>
  requires(avnd::raw_file_input_introspection<T>::size > 0)
struct raw_file_input_storage<T>
{
  using hdl_tuple = avnd::filter_and_apply<
      raw_file_handle_type, avnd::raw_file_input_introspection, T>;

  // std::tuple< std::shared_ptr<raw_file_data> >
  AVND_NO_UNIQUE_ADDRESS hdl_tuple handles;
};

template <typename T>
struct raw_file_storage : raw_file_input_storage<T>
{
  void init(avnd::effect_container<T>& t) { }

  //! \see soundfile_storage::load
  template <std::size_t N, std::size_t NField>
  std::shared_ptr<raw_file_data> load(
      avnd::effect_container<T>& t, const std::shared_ptr<raw_file_data>& hdl,
      avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    std::shared_ptr<raw_file_data>& g = get<N>(this->handles);

    // The previous handle has to stay alive until the ports have been
    // repointed below; it is then given back to the caller so that it is not
    // freed in whichever thread called us.
    std::shared_ptr<raw_file_data> previous = std::exchange(g, hdl);

    for(auto state : t.full_state())
    {
      avnd::raw_file_port auto& port = avnd::pfr::get<NField>(state.inputs);

      const bool changed = port.file.filename != hdl->filename;

      // The views are always repointed to the new handle, even when the file
      // name did not change: they would otherwise dangle as soon as the
      // previous handle is released at the end of this function.
      port.file.bytes
          = decltype(port.file.bytes)(hdl->data.constData(), hdl->file.size());
      port.file.filename = hdl->filename;

      if(changed)
      {
        if_possible(port.update(state.effect));
      }
    }
    return previous;
  }

  //! \see soundfile_storage::load
  template <std::size_t N, std::size_t NField>
  std::shared_ptr<raw_file_data> load(
      T& state, const std::shared_ptr<raw_file_data>& hdl, avnd::predicate_index<N>,
      avnd::field_index<NField>)
  {
    std::shared_ptr<raw_file_data>& g = get<N>(this->handles);

    // The previous handle has to stay alive until the ports have been
    // repointed below; it is then given back to the caller so that it is not
    // freed in whichever thread called us.
    std::shared_ptr<raw_file_data> previous = std::exchange(g, hdl);

    // FIXME not generic enough.. GPU should also use effect_container
    avnd::raw_file_port auto& port = avnd::pfr::get<NField>(state.inputs);

    const bool changed = port.file.filename != hdl->filename;

    port.file.bytes
        = decltype(port.file.bytes)(hdl->data.constData(), hdl->file.size());
    port.file.filename = hdl->filename;

    if(changed)
    {
      if_possible(port.update(state));
    }
    return previous;
  }
};
}
#endif
