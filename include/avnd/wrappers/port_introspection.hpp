#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/concepts.hpp>
#include <boost/pfr.hpp>
#include <avnd/common/struct_reflection.hpp>

namespace avnd
{

/* Not possible until clang-13 :"(
 *
template<auto F, typename T>
using concept_to_mp_bool = boost::mp11::mp_bool<F.template operator()<T>()>;

#define CONCEPT(TheConcept) \
  [] <typename T> () consteval { return TheConcept<T>; }

static_assert(check<CONCEPT(std::floating_point), float>::value);
*/

template <typename Field>
using is_parameter_t = boost::mp11::mp_bool<parameter<Field>>;
template <typename T>
using parameter_introspection = predicate_introspection<T, is_parameter_t>;

template <typename Field>
using is_float_parameter_t = boost::mp11::mp_bool<float_parameter<Field>>;
template <typename T>
using float_parameter_introspection
    = predicate_introspection<T, is_float_parameter_t>;

template <typename Field>
using is_midi_port_t = boost::mp11::mp_bool<midi_port<Field>>;
template <typename T>
using midi_port_introspection = predicate_introspection<T, is_midi_port_t>;

template <typename Field>
using is_raw_container_midi_port_t
    = boost::mp11::mp_bool<raw_container_midi_port<Field>>;
template <typename T>
using raw_container_midi_port_introspection
    = predicate_introspection<T, is_raw_container_midi_port_t>;

template <typename Field>
using is_audio_bus_t = boost::mp11::mp_bool<poly_audio_port<Field>>;
template <typename T>
using audio_bus_introspection = predicate_introspection<T, is_audio_bus_t>;

template <typename Field>
using is_audio_channel_t = boost::mp11::mp_bool<mono_audio_port<Field>>;
template <typename T>
using audio_channel_introspection
    = predicate_introspection<T, is_audio_channel_t>;

// template <typename Field>
// using is_message_t = boost::mp11::mp_bool<message<Field>>;
// template <typename T>
// using message_introspection = predicate_introspection<T, is_message_t>;

template <typename Field>
using is_callback_t = boost::mp11::mp_bool<callback<Field>>;
template <typename T>
using callback_introspection = predicate_introspection<T, is_callback_t>;

}
