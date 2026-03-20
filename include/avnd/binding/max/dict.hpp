#pragma once
#include <avnd/introspection/generic.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/generic.hpp>

#include <ext_dictionary.h>
#include <ext_dictobj.h>

namespace max
{
template <typename T>
concept dict_parameter
    = avnd::parameter_port<T> &&
      (avnd::has_field_names<decltype(T::value)> || avnd::dict_ish<decltype(T::value)>);

generate_predicate_introspection(dict_parameter);
generate_member_introspection(dict_parameter, inputs);
generate_member_introspection(dict_parameter, outputs);

struct dict_state
{
  dict_state()
      : d{dictionary_new()}
  {
  }
  dict_state(const dict_state&) = delete;
  dict_state& operator=(const dict_state&) = delete;
  dict_state(dict_state&&) = delete;
  dict_state& operator=(dict_state&&) = delete;

  ~dict_state() { object_free(d); }

  t_dictionary* d{};
  t_symbol* s{};
};

generate_port_state_holders(dict_parameter, dict_state);

template <typename T>
struct dict_storage
{
  AVND_NO_UNIQUE_ADDRESS
  dict_state_input_storage<T> inputs;
  AVND_NO_UNIQUE_ADDRESS
  dict_state_output_storage<T> outputs;

  void init(avnd::effect_container<T>& t)
  {
    if constexpr(dict_parameter_outputs_introspection<T>::size > 0)
      dict_parameter_outputs_introspection<T>::for_all_n(
          avnd::get_outputs(t),
          [this]<typename F, std::size_t N>(F& field, avnd::predicate_index<N>) {
        auto& state = get<N>(outputs.handles);
        dictobj_register(state.d, &state.s);
      });
  }

  void release(avnd::effect_container<T>& t)
  {
    if constexpr(dict_parameter_outputs_introspection<T>::size > 0)
      dict_parameter_outputs_introspection<T>::for_all_n(
          avnd::get_outputs(t),
          [this]<typename F, std::size_t N>(F& field, avnd::predicate_index<N>) {
        auto& state = get<N>(outputs.handles);
        dictobj_release(state.d);
      });
  }

  template <std::size_t NField>
  auto& get_input(avnd::field_index<NField>)
  {
    constexpr std::size_t NPred
        = dict_parameter_outputs_introspection<T>::field_index_to_index(
            avnd::field_index<NField>{});
    return get<NPred>(inputs.handles);
  }

  template <std::size_t NField>
  auto& get_output(avnd::field_index<NField>)
  {
    constexpr std::size_t NPred
        = dict_parameter_outputs_introspection<T>::field_index_to_index(
            avnd::field_index<NField>{});
    return get<NPred>(outputs.handles);
  }
};
}
