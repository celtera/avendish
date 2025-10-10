#pragma once
#include <avnd/binding/ossia/node.hpp>
#include <avnd/concepts/temporality.hpp>

namespace oscr
{

// Special case for the easy non-audio case
template <ossia_compatible_nonaudio_processor T>
  requires(!avnd::tag_cv<T>)
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
  requires(avnd::tag_cv<T>)
class safe_node<T> : public safe_node_base<T, safe_node<T>>
{
public:
  using safe_node_base<T, safe_node<T>>::safe_node_base;

  bool exec_once{};

  constexpr bool scan_audio_input_channels() { return false; }
  // This function goes from a host-provided tick to what the plugin expects
  template <typename Tick>
  auto invoke_effect(int index, int N, auto&& val, const Tick& t)
  {
    T* pobj{};
    if constexpr(avnd::tag_stateless<T>)
    {
      pobj = &this->impl.effect;
    }
    else
    {
      this->impl.effect.reserve(N);
      while(this->impl.effect.size() < N)
      {
        // FIXME this should be done before prepare, port copy, etc
        if(this->impl.effect.empty())
          this->impl.effect.resize(1);
        else
          this->impl.effect.push_back(this->impl.effect[0]);
      }
      pobj = &this->impl.effect[index];
    }
    auto& obj = *pobj;
    // clang-format off
    if constexpr(std::is_integral_v<Tick>)
    {
      static_assert(!avnd::has_tick<T>);
      if_possible_r(obj(val, t))
 else if_possible_r(obj(val))
 else static_assert(std::is_void_v<Tick>, "impossible to call");
    }
    else
    {
      if constexpr (avnd::has_tick<T>)
      {
        // Do the process call
        if_possible_r(obj(val, t))
   else if_possible_r(obj(val, t.frames))
   else if_possible_r(obj(val, t.frames, t))
   else if_possible_r(obj(val))
   else static_assert(std::is_void_v<Tick>, "impossible to call");
      }
      else
      {
        if_possible_r(obj(val, t.frames))
   else if_possible_r(obj(val))
   else static_assert(std::is_void_v<Tick>, "impossible to call");
      }
    }
    // clang-format on
  }

  using input_value_type = std::remove_cvref_t<
      boost::mp11::mp_first<typename avnd::function_reflection_o<T>::arguments>>;
  using operator_ret = typename avnd::function_reflection_o<T>::return_type;

  template <typename ValType, typename Tick>
  struct process_value_no_ret
  {
    safe_node& self;
    const Tick& tick;
    int ts{};
    void operator()() { }
    void operator()(ossia::impulse) { self.invoke_effect(0, 1, 0, tick); }
    void operator()(bool v) { self.invoke_effect(0, 1, v ? 1 : 0, tick); }
    void operator()(int v) { self.invoke_effect(0, 1, v, tick); }
    void operator()(float v) { self.invoke_effect(0, 1, v, tick); }
    void operator()(const std::string& v)
    {
      self.invoke_effect(0, 1, std::stof(v), tick);
    }
    template <std::size_t N>
    void operator()(std::array<float, N> v)
    {
      std::array<float, N> res;
      for(int i = 0; i < N; i++)
        res[i] = self.invoke_effect(i, N, v[i], tick);
    }

    // FIXME handle recursion
    void operator()(const std::vector<ossia::value>& v)
    {
      for(std::size_t i = 0, N = v.size(); i < N; i++)
        self.invoke_effect(i, N, ossia::convert<float>(v[i]), tick);
    }
    void operator()(const ossia::value_map_type& v)
    {
      int N = v.size();
      int i = 0;
      for(auto& [k, val] : v)
      {
        self.invoke_effect(i++, N, ossia::convert<float>(val), tick);
      }
    }
  };

  template <typename ValType, typename Tick>
  struct process_value
  {
    safe_node& self;
    const Tick& tick;
    ossia::value_port& out;
    int ts{};
    void operator()() { }
    void operator()(ossia::impulse)
    {
      out.write_value(self.invoke_effect(0, 1, 0, tick), 0);
    }
    void operator()(bool v)
    {
      out.write_value(self.invoke_effect(0, 1, v ? 1 : 0, tick), 0);
    }
    void operator()(int v) { out.write_value(self.invoke_effect(0, 1, v, tick), 0); }
    void operator()(float v) { out.write_value(self.invoke_effect(0, 1, v, tick), 0); }
    void operator()(const std::string& v)
    {
      out.write_value(self.invoke_effect(0, 1, std::stof(v), tick), 0);
    }
    template <std::size_t N>
    void operator()(std::array<float, N> v)
    {
      std::array<float, N> res;
      for(int i = 0; i < N; i++)
        res[i] = self.invoke_effect(i, N, v[i], tick);
      out.write_value(res, 0);
    }

    // FIXME handle recursion
    void operator()(const std::vector<ossia::value>& v)
    {
      std::vector<ossia::value> res;
      res.reserve(v.size());

      for(std::size_t i = 0, N = v.size(); i < N; i++)
        res.push_back(self.invoke_effect(i, N, ossia::convert<ValType>(v[i]), tick));

      out.write_value(std::move(res), 0);
    }
    void operator()(const ossia::value_map_type& v)
    {
      int N = v.size();
      int i = 0;
      ossia::value_map_type res;
      for(auto& [k, val] : v)
      {
        res.emplace_back(
            k, self.invoke_effect(i++, N, ossia::convert<ValType>(val), tick));
      }
      out.write_value(std::move(res), 0);
    }
  };

  template <typename ValType, typename Tick>
  struct process_value_opt
  {
    safe_node& self;
    const Tick& tick;
    ossia::value_port& out;
    int ts{};
    void operator()() { }
    void operator()(ossia::impulse)
    {
      if(auto res = self.invoke_effect(0, 1, 0, tick))
        out.write_value(*res, 0);
    }
    void operator()(bool v)
    {
      if(auto res = self.invoke_effect(0, 1, v ? 1 : 0, tick))
        out.write_value(*res, 0);
    }
    void operator()(int v)
    {
      if(auto res = self.invoke_effect(0, 1, v, tick))
        out.write_value(*res, 0);
    }
    void operator()(float v)
    {
      if(auto res = self.invoke_effect(0, 1, v, tick))
        out.write_value(*res, 0);
    }
    void operator()(const std::string& v)
    {
      if(auto res = self.invoke_effect(0, 1, std::stof(v), tick))
        out.write_value(*res, 0);
    }
    template <std::size_t N>
    void operator()(std::array<float, N> v)
    {
      // FIXME
      //      std::array<float, N> res;
      //      for(int i = 0; i < N; i++)
      //        res[i] = self.invoke_effect(self.impl.effect, v[i], tick);
      //      out.write_value(res, 0);
    }

    // FIXME handle recursion
    void operator()(const std::vector<ossia::value>& v)
    {
      /*
      std::vector<ossia::value> res;
      res.reserve(v.size());

      for(std::size_t i = 0; i < v.size(); i++)
        res.push_back(self.invoke_effect(self.impl.effect, ossia::convert<float>(v[i]), tick));

      out.write_value(std::move(res), 0);
*/
    }
    void operator()(const ossia::value_map_type& v)
    {
      /*
      ossia::value_map_type res;
      for(auto& [k, val] : v)
      {
        res.emplace_back(k, self.invoke_effect(self.impl.effect, ossia::convert<float>(val), tick));
      }
      out.write_value(std::move(res), 0);
*/
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

    if constexpr(std::is_void_v<operator_ret>)
    {
      if constexpr(std::is_same_v<input_value_type, ossia::value>)
      {
        for(const ossia::timed_value& val : this->arg_value_ports.in->get_data())
        {
          invoke_effect(this->impl.effect, val.value, tick);
        }
      }
      else
      {
        process_value_no_ret<input_value_type, decltype(tick)> proc{
            *this, tick, *this->arg_value_ports.out};
        for(const ossia::timed_value& val : this->arg_value_ports.in->get_data())
        {
          val.value.apply(proc);
        }
      }
    }
    else if constexpr(avnd::optional_ish<operator_ret>)
    {
      if constexpr(std::is_same_v<input_value_type, ossia::value>)
      {
        for(const ossia::timed_value& val : this->arg_value_ports.in->get_data())
        {
          if(auto res = invoke_effect(this->impl.effect, val.value, tick))
            this->arg_value_ports.out->write_value(*res, 0);
        }
      }
      else
      {
        process_value_opt<input_value_type, decltype(tick)> proc{
            *this, tick, *this->arg_value_ports.out};
        for(const ossia::timed_value& val : this->arg_value_ports.in->get_data())
        {
          val.value.apply(proc);
        }
      }
    }
    else
    {
      if constexpr(std::is_same_v<input_value_type, ossia::value>)
      {
        for(const ossia::timed_value& val : this->arg_value_ports.in->get_data())
        {
          this->arg_value_ports.out->write_value(
              invoke_effect(0, 1, val.value, tick), 0);
        }
      }
      else
      {
        process_value<input_value_type, decltype(tick)> proc{
            *this, tick, *this->arg_value_ports.out};
        for(const ossia::timed_value& val : this->arg_value_ports.in->get_data())
        {
          val.value.apply(proc);
        }
      }
    }
    this->finish_run();
  }
};
}
