#pragma once
#include <avnd/helpers/controls.hpp>

namespace avnd
{

template<typename T>
struct sample_accurate_values
{
  std::map<int, T> values;
};

namespace sample_accurate
{

template<static_string lit, typename T>
struct value_port
{
  static consteval auto name() { return std::string_view{lit.value}; }

  operator T&() noexcept { return value; }
  operator T() const noexcept { return value; }
  auto& operator=(T t) noexcept { value = t; return *this; }

  const auto& operator[](int frame) const noexcept {
    return values[frame];
  }

  // Running value (last value before the tick started)
  T value;

  // Values for this tick
  std::map<int, T> values;
};


template <static_string lit>
struct maintained_button
  : avnd::maintained_button<lit>
  , sample_accurate_values<avnd::impulse>
{

};

template <static_string lit>
struct impulse_button
        : avnd::impulse_button<lit>
        , sample_accurate_values<avnd::impulse>
{

};

template <static_string lit, toggle_setup setup>
struct toggle_t
        : avnd::toggle_t<lit, setup>
{

};

template <static_string lit, range setup = default_range<float>>
struct hslider_f32
  : avnd::hslider_f32<lit, setup>
  , sample_accurate_values<float>
{

};

template <static_string lit, range setup = default_range<float>>
struct hslider_i32
  : avnd::hslider_f32<lit, setup>
  , sample_accurate_values<int>
{

};

template <static_string lit, range setup = default_range<float>>
struct vslider_f32
  : avnd::vslider_f32<lit, setup>
  , sample_accurate_values<float>
{

};

template <static_string lit, range setup = default_range<float>>
struct vslider_i32
  : avnd::vslider_f32<lit, setup>
  , sample_accurate_values<int>
{

};

}

template<typename T>
struct accurate
  : T
  , sample_accurate_values<std::decay_t<decltype(T::value)>>
{

};

}
