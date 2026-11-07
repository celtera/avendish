#pragma once
#include <avnd/introspection/input.hpp>

#include <type_traits>
namespace oscr
{
//! A time chooser that lets the user choose what its synced steps do at a bar
//! line (halp::bar_sync): `offers_bar_sync` a constant expression that is
//! true. Any other field, one with a non-constant member of that name too, is
//! not one.
template <typename Field>
concept offers_bar_sync = requires {
  typename std::bool_constant<bool(Field::offers_bar_sync)>;
} && bool(Field::offers_bar_sync);

//! One whose new controls keep one pulse across the bar lines.
template <typename Field>
concept starts_as_pulse = offers_bar_sync<Field> && requires {
  typename std::bool_constant<bool(Field::bar_sync_default)>;
} && !bool(Field::bar_sync_default);

struct time_control_state
{
  float value{};
  bool sync{};
};

template <typename Field>
struct time_control_state_type;

template <avnd::time_control_port Field>
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

  AVND_NO_UNIQUE_ADDRESS hdl_tuple handles;
};

//! A synced time chooser's mode, the second component of its {value, mode}:
//! 3 keeps one pulse across the bar lines, anything else restarts the steps
//! at each of them. 1 is what a value carries when it was saved before the
//! choice existed, when the steps always restarted, so it keeps doing so;
//! 2 is the restart chosen.
static constexpr bool bar_sync_of(float mode) noexcept
{
  return !(mode > 2.5f && mode < 3.5f);
}

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
        = avnd::time_control_input_introspection<T>::field_index_to_index(
            avnd::field_index<NField>{});

    time_control_state& g = get<NPred>(this->handles);
    g = {.value = value, .sync = sync};
  }
};
}
