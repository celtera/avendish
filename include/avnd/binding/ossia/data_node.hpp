#pragma once
#include <avnd/binding/ossia/node.hpp>
#include <avnd/concepts/temporality.hpp>

namespace oscr
{

// Special case for the easy non-audio case
template <ossia_compatible_nonaudio_processor T>
  requires(!(avnd::tag_cv<T> && avnd::tag_stateless<T>))
class safe_node<T> : public safe_node_base<T, safe_node<T>>
{
public:
  using safe_node_base<T, safe_node<T>>::safe_node_base;

  bool exec_once{};

  constexpr bool scan_audio_input_channels() { return false; }

  OSSIA_MAXIMUM_INLINE
  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    // FIXME handle splitting execution multiple times per-input for e.g. time_independent mapping objects
    if constexpr(avnd::tag_single_exec<T>)
    {
      if(std::exchange(exec_once, true))
      {
        return;
      }
    }

    auto [start, frames] = st.timings(tk);

    if(!this->prepare_run(tk, st, start, frames))
    {
      this->finish_run();
      return;
    }

    // Smooth
    this->process_smooth();

    avnd::invoke_effect(
        this->impl,
        avnd::get_tick_or_frames(this->impl, tick_info{*this, tk, st, frames}));

    this->finish_run();
  }
};

template <ossia_compatible_nonaudio_processor T>
  requires(avnd::tag_cv<T> && avnd::tag_stateless<T>)
class safe_node<T> : public safe_node_base<T, safe_node<T>>
{
public:
  using safe_node_base<T, safe_node<T>>::safe_node_base;

  bool exec_once{};

  constexpr bool scan_audio_input_channels() { return false; }
  // This function goes from a host-provided tick to what the plugin expects
  template <typename Tick>
  auto invoke_effect(auto&& val, const Tick& t)
  {
    return this->impl.effect(val);
    /*
    // clang-format off
    if constexpr(std::is_integral_v<Tick>)
    {
      static_assert(!avnd::has_tick<T>);
      if_possible_r(this->impl.effect(val, t))
 else if_possible_r(this->impl.effect(val))
 else static_assert(std::is_void_v<Tick>, "impossible to call");
    }
    else
    {
      if constexpr (avnd::has_tick<T>)
      {
        // Do the process call
        if_possible_r(this->impl.effect(val, t))
   else if_possible_r(this->impl.effect(val, t.frames))
   else if_possible_r(this->impl.effect(val, t.frames, t))
   else if_possible_r(this->impl.effect(val))
   else static_assert(std::is_void_v<Tick>, "impossible to call");
      }
      else
      {
        if_possible_r(this->impl.effect(val, t.frames))
   else if_possible_r(this->impl.effect(val))
   else static_assert(std::is_void_v<Tick>, "impossible to call");
      }
    }
    // clang-format on
*/
  }

  template <typename Tick>
  struct process_value
  {
    safe_node& self;
    const Tick& tick;
    ossia::value_port& out;
    int ts{};
    void operator()() { }
    void operator()(ossia::impulse) { out.write_value(self.invoke_effect(0, tick), 0); }
    void operator()(bool v) { out.write_value(self.invoke_effect(v ? 1 : 0, tick), 0); }
    void operator()(int v) { out.write_value(self.invoke_effect(v, tick), 0); }
    void operator()(float v) { out.write_value(self.invoke_effect(v, tick), 0); }
    void operator()(const std::string& v)
    {
      out.write_value(self.invoke_effect(std::stof(v), tick), 0);
    }
    template <std::size_t N>
    void operator()(std::array<float, N> v)
    {
      std::array<float, N> res;
      for(int i = 0; i < N; i++)
        res[i] = self.invoke_effect(v[i], tick);
      out.write_value(res, 0);
    }

    // FIXME handle recursion
    void operator()(const std::vector<ossia::value>& v)
    {
      std::vector<ossia::value> res;
      res.reserve(v.size());

      for(std::size_t i = 0; i < v.size(); i++)
        res.push_back(self.invoke_effect(ossia::convert<float>(v[i]), tick));

      out.write_value(std::move(res), 0);
    }
    void operator()(const ossia::value_map_type& v)
    {
      ossia::value_map_type res;
      for(auto& [k, val] : v)
      {
        res.emplace_back(k, self.invoke_effect(ossia::convert<float>(val), tick));
      }
      out.write_value(std::move(res), 0);
    }
  };

  OSSIA_MAXIMUM_INLINE
  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    // FIXME handle splitting execution multiple times per-input for e.g. time_independent mapping objects
    if constexpr(avnd::tag_single_exec<T>)
    {
      if(std::exchange(exec_once, true))
      {
        return;
      }
    }

    auto [start, frames] = st.timings(tk);

    if(!this->prepare_run(tk, st, start, frames))
    {
      this->finish_run();
      return;
    }

    // Smooth
    this->process_smooth();

    const auto tick
        = avnd::get_tick_or_frames(this->impl, tick_info{*this, tk, st, frames});
    process_value<decltype(tick)> proc{*this, tick, *this->arg_value_ports.out};

    for(const ossia::timed_value& val : this->arg_value_ports.in->get_data())
    {
      val.value.apply(proc);
    }

    this->finish_run();
  }
};
}
