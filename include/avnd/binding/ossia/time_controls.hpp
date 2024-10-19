#pragma once
#include <avnd/introspection/input.hpp>
namespace oscr
{
struct time_control_state
{
  float value{};
  bool sync{};
};

template <typename Field>
struct time_control_state_type;

template <avnd::time_control Field>
struct time_control_state_type<Field> : time_control_state
{
};

template <typename T>
struct time_control_input_storage
{
};

template <typename T>
  requires(avnd::time_control_input_introspection<T>::size > 0)
struct time_control_input_storage<T>
{
  using hdl_tuple = avnd::filter_and_apply<
      time_control_state_type, avnd::time_control_input_introspection, T>;

  [[no_unique_address]] hdl_tuple handles;
};

static constexpr float to_seconds(float ratio, float tempo) noexcept
{
  const float q_ratio = 4. * ratio; // e.g 1. for a quarter note, 4. for a whole
  const float beat_dur = 60. / tempo;
  return q_ratio * beat_dur;
}

template <typename T>
struct time_control_storage : time_control_input_storage<T>
{
  void init(avnd::effect_container<T>& t) { }

  template <std::size_t N, std::size_t NField>
  void set_tempo(
      avnd::effect_container<T>& t, avnd::predicate_index<N>, avnd::field_index<NField>,
      double new_tempo)
  {
    if(new_tempo <= 0.)
      return;

    auto& g = get<N>(this->handles);
    if(g.sync)
    {
      for(auto state : t.full_state())
      {
        auto& port = avnd::pfr::get<NField>(state.inputs);
        port.value = to_seconds(g.value, new_tempo);
        if_possible(port.update(state.effect));
      }
    }
  }

  template <std::size_t NField>
  void update_control(
      avnd::effect_container<T>& t, avnd::field_index<NField>, float value, bool sync)
  {
    static constexpr std::size_t NPred
        = avnd::time_control_input_introspection<T>::template field_index_to_index(
            avnd::field_index<NField>{});

    time_control_state& g = get<NPred>(this->handles);
    g = {.value = value, .sync = sync};
  }
};
}
