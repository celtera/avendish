#pragma once
#include <avnd/common/struct_reflection.hpp>
// #include <halp/midi.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

namespace oscr
{
template <typename T>
constexpr bool vecf_compatible()
{
#define is_fp(X) std::is_floating_point_v<std::decay_t<decltype(X)>>
  constexpr int sz = avnd::pfr::tuple_size_v<T>;
  if constexpr (sz == 2)
   {
     auto [x, y] = T{};
     return is_fp(x) && is_fp(y);
   }
   else if constexpr (sz == 3)
   {
     auto [x, y, z] = T{};
     return is_fp(x) && is_fp(y) && is_fp(z);
   }
   else if constexpr (sz == 4)
   {
     auto [x, y, z, w] = T{};
     return is_fp(x) && is_fp(y) && is_fp(z) && is_fp(w);
   }
#undef is_fp
  return false;
}

struct to_ossia_value_impl
{
  ossia::value& val;

  template<typename F>
  void to_vector(const F& f)
  {
    constexpr int fields = avnd::pfr::tuple_size_v<F>;
    std::vector<ossia::value> v;
    v.resize(fields);

    int k = 0;
    avnd::pfr::for_each_field(f, [&] (const auto& f) {
      to_ossia_value_impl{v[k++]}(f);
    });

    val = std::move(v);
  }

  template<typename F>
  requires std::is_aggregate_v<F>
  void operator()(const F& f)
  {
    constexpr int fields = avnd::pfr::tuple_size_v<F>;
    if constexpr(vecf_compatible<F>())
    {
      if constexpr (fields == 2)
      {
        auto [x, y] = f;
        val = ossia::vec2f{x, y};
      }
      else if constexpr (fields == 3)
      {
        auto [x, y, z] = f;
        val = ossia::vec3f{x, y, z};
      }
      else if constexpr (fields == 4)
      {
        auto [x, y, z, w] = f;
        val = ossia::vec4f{x, y, z, w};
      }
      else
      {
        to_vector(f);
      }
    }
    else
    {
      to_vector(f);
    }
  }

  template<typename F>
  requires (std::is_integral_v<F>)
  void operator()(const F& f)
  {
    val = (int)f;
  }

  template<typename F>
  requires (std::is_floating_point_v<F>)
  void operator()(const F& f)
  {
    val = (float)f;
  }

  template<typename... Args>
  void operator()(const std::variant<Args...>& f)
  {
    std::visit([&] (const auto &arg) { (*this)(arg); }, f);
  }

  template<typename T>
  void operator()(const std::vector<T>& f)
  {
    std::vector<ossia::value> v;
    v.resize(f.size());
    for(int i = 0, n = f.size(); i < n; i++)
      to_ossia_value_impl{v[i]}(f[i]);
    val = std::move(v);
  }

  template<std::floating_point T>
  void operator()(const T (&f)[2])
  { val = ossia::vec2f{f[0], f[1]}; }

  template<std::floating_point T>
  void operator()(const T (&f)[3])
  { val = ossia::vec3f{f[0], f[1], f[2]}; }

  template<std::floating_point T>
  void operator()(const T (&f)[4])
  { val = ossia::vec4f{f[0], f[1], f[2], f[3]}; }

  void operator()(const auto& f)
  {
    val = f;
  }
};
template <typename T>
ossia::value to_ossia_value_rec(T&& v)
{
   //static_assert(std::is_void_v<T>, "unsupported case");
   ossia::value val;
   to_ossia_value_impl{val}(v);
   return val;
}


template <typename T>
ossia::value to_ossia_value(T&& v)
{
  using type = std::decay_t<T>;
  constexpr int sz = avnd::pfr::tuple_size_v<type>;
  if constexpr (sz == 0)
  {
    return ossia::impulse{};
  }
  else if constexpr(vecf_compatible<type>())
  {
    if constexpr (sz == 2)
    {
      auto [x, y] = v;
      return ossia::vec2f{x, y};
    }
    else if constexpr (sz == 3)
    {
      auto [x, y, z] = v;
      return ossia::vec3f{x, y, z};
    }
    else if constexpr (sz == 4)
    {
      auto [x, y, z, w] = v;
      return ossia::vec4f{x, y, z, w};
    }
    else
    {
      return to_ossia_value_rec(std::forward<T>(v));
    }
  }
  else
  {
    return to_ossia_value_rec(std::forward<T>(v));
  }
}

template <typename T, std::size_t N>
ossia::value to_ossia_value(const T (&v)[N])
{
  if constexpr(std::is_floating_point_v<T>)
  {
    if constexpr (N == 2)
    {
      auto [x, y] = v;
      return ossia::vec2f{x, y};
    }
    else if constexpr (N == 3)
    {
      auto [x, y, z] = v;
      return ossia::vec3f{x, y, z};
    }
    else if constexpr (N == 4)
    {
      auto [x, y, z, w] = v;
      return ossia::vec4f{x, y, z, w};
    }
    else
    {
      return std::vector<ossia::value>(std::begin(v), std::end(v));
    }
  }
  else
  {
    return std::vector<ossia::value>(std::begin(v), std::end(v));
  }
}

template <typename T, std::size_t N>
ossia::value to_ossia_value(const std::array<T, N>& v)
{
  const auto& [elem] = v;
  return to_ossia_value(elem);
}

template <typename T>
requires (requires { T{}.begin() != T{}.end(); } && !avnd::string_ish<T>)
ossia::value to_ossia_value(const T& v)
{
  // C++23: ranges::to (thanks cor3ntin!)
  std::vector<ossia::value> vec;
  for(auto& e : v)
    vec.push_back(to_ossia_value(std::move(e)));
  return vec;
}

ossia::value to_ossia_value(std::integral auto v)
{
  return v;
}
ossia::value to_ossia_value(std::floating_point auto v)
{
  return v;
}
ossia::value to_ossia_value(avnd::string_ish auto v)
{
  return std::string{v};
}
ossia::value to_ossia_value(avnd::enum_ish auto v)
{
  return static_cast<int>(v);
}
template <typename T>
requires std::is_enum_v<T> ossia::value to_ossia_value(T v)
{
  return static_cast<int>(v);
}

inline ossia::value to_ossia_value(bool v)
{
  return v;
}

template <typename Exec_T, typename Obj_T>
struct process_after_run
{
  Exec_T& self;
  Obj_T& impl;

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::texture_inlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::parameter Field, std::size_t Idx>
  requires(!avnd::control<Field>)
  void write_value(
      Field& ctrl,
      ossia::value_outlet& port,
      auto& val,
      int64_t ts,
      avnd::field_index<Idx>) const noexcept
  {
    port->write_value(val, ts);
  }

  template <avnd::parameter Field, std::size_t Idx>
  requires(avnd::control<Field>)
  void write_value(
      Field& ctrl,
      ossia::value_outlet& port,
      auto& val,
      int64_t ts,
      avnd::field_index<Idx> idx) const noexcept
  {
    port->write_value(val, ts);

    // Get the index of the control in [0; N[
    using type = typename Exec_T::processor_type;
    using controls = avnd::control_output_introspection<type>;
    constexpr int control_index = controls::field_index_to_index(idx);

    // Mark the control as changed
    self.control.outputs_set.set(control_index);
  }

  template <avnd::parameter Field, std::size_t Idx>
  requires(!avnd::sample_accurate_parameter<Field>)
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    write_value(ctrl, port, ctrl.value, 0, avnd::field_index<Idx>{});
  }

  template <avnd::linear_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx> idx) const noexcept
  {
    auto& buffers = self.control_buffers.linear_inputs;
    // Idx is the index of the port in the complete input array.
    // We need to map it to the linear input index.
    using processor_type = typename Exec_T::processor_type;
    using lin_out = avnd::linear_timed_parameter_output_introspection<processor_type>;
    constexpr int storage_index = lin_out::field_index_to_index(idx);

    auto& buffer = get<storage_index>(buffers);

    for (int i = 0, N = self.buffer_size; i < N; i++)
    {
      if (buffer[i])
      {
        write_value(ctrl, port, *buffer[i], i, avnd::field_index<Idx>{});
        buffer[i] = {};
      }
    }
  }

  template <avnd::dynamic_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    for (auto& [timestamp, val] : ctrl.values)
    {
      write_value(ctrl, port, val, timestamp, avnd::field_index<Idx>{});
    }
    ctrl.values.clear();
  }

  // does not make sense as output, only as input
  template <avnd::span_sample_accurate_parameter Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>)
      const noexcept = delete;

  template <typename Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::audio_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::raw_container_midi_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    const int N = ctrl.midi_messages.size;
    port.data.messages.clear();
    port.data.messages.reserve(N);
    for (int i = 0; i < N; i++)
    {
      auto& m = ctrl.midi_messages[i];
      libremidi::message ms;
      ms.bytes.assign(m.bytes.begin(), m.bytes.end());
      ms.timestamp = m.timestamp;
      port.data.messages.push_back(std::move(ms));
    }
  }

  template <avnd::dynamic_container_midi_port Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::midi_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    port.data.messages.clear();
    port.data.messages.reserve(ctrl.midi_messages.size());
    for (auto& m : ctrl.midi_messages)
    {
      libremidi::message ms;
      ms.bytes.assign(m.bytes.begin(), m.bytes.end());
      ms.timestamp = m.timestamp;
      port.data.messages.push_back(std::move(ms));
    }
  }

  template <typename Field, std::size_t Idx>
  void
  operator()(Field& ctrl, ossia::texture_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }

  template <avnd::callback Field, std::size_t Idx>
  void operator()(Field& ctrl, ossia::value_outlet& port, avnd::field_index<Idx>) const noexcept
  {
  }
};

}
