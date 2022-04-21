#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/all.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <boost/mp11.hpp>

namespace avnd
{
// Field: struct { struct { float** data; } soundfile; }
// This gives us: float*
template <typename Field>
using soundfile_channel_type
    = std::remove_pointer_t<std::remove_reference_t<decltype(Field{}.soundfile.data)>>;

template <typename T>
struct soundfile_input_storage
{
};

template <typename T>
requires(soundfile_input_introspection<T>::size > 0)
struct soundfile_input_storage<T>
{
  // std::tuple< float*, double* >
  using tuple = filter_and_apply<
      soundfile_channel_type,
      soundfile_input_introspection,
      T>;

  // std::tuple< std::vector<float*>, std::vector<double*> >
  using vectors = boost::mp11::mp_transform<std::vector, tuple>;

  [[no_unique_address]] vectors soundfiles;
};


/**
 * Used to store RAM-loaded soundfiles channel pointers
 */
template <typename T>
struct soundfile_storage
    : soundfile_input_storage<T>
{
  using sf_in = soundfile_input_introspection<T>;

  void init(avnd::effect_container<T>& t)
  {
    if constexpr (sf_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M & port, avnd::num<Idx>)
      {
        // Get the matching buffer in our storage, a std::vector<timed_value>
        auto& buf = tpl::get<Idx>(this->soundfiles);

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
};

}
