#pragma once
#include <avnd/binding/ossia/node.hpp>
#include <avnd/concepts/temporality.hpp>

namespace oscr
{

// Special case for the easy non-audio case
template <ossia_compatible_nonaudio_processor T>
class safe_node<T> : public safe_node_base<T, safe_node<T>>
{
public:
  using safe_node_base<T, safe_node<T>>::safe_node_base;

  bool exec_once{};

  constexpr bool scan_audio_input_channels() { return false; }

  OSSIA_MAXIMUM_INLINE
  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    if constexpr(avnd::tag_single_exec<T>)
    {
      if(std::exchange(exec_once, true))
      {
        return;
      }
    }

    auto [start, frames] = st.timings(tk);

    if(!this->prepare_run(tk, start, frames))
    {
      this->finish_run();
      return;
    }

    // Smooth
    this->process_smooth();

    avnd::invoke_effect(
        this->impl, avnd::get_tick_or_frames(this->impl, tick_info{tk, st, frames}));

    this->finish_run();
  }
};
}
