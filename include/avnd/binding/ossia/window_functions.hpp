#pragma once
#include <cmath>

namespace oscr
{

struct hanning_window
{
  template <typename T>
  void operator()(const T* in, T* out, int size)
  {
    const T norm = 0.5 / size;
    const auto factor = ossia::two_pi / (size - 1);
    for(int i = 0; i < size; ++i)
    {
      out[i] = in[i] * norm * (1. - std::cos(factor * i));
    }
  }
};

struct hamming_window
{
  template <typename T>
  void operator()(const T* in, T* out, int size)
  {
    constexpr T a0 = 25. / 46.;
    constexpr T a1 = 21. / 46.;

    const T norm = 1. / size;
    const auto factor = ossia::two_pi / (size - 1);
    for(int i = 0; i < size; ++i)
    {
      out[i] = in[i] * norm * (a0 - a1 * std::cos(factor * i));
    }
  }
};

struct apply_window
{
  template <typename Field, typename T>
  void operator()(Field& f, const T* in, T* out, int frames) noexcept
  {
    if constexpr(requires { Field::window::hanning; })
    {
      hanning_window{}(in, out, frames);
    }
    else if constexpr(requires { Field::window::hamming; })
    {
      hamming_window{}(in, out, frames);
    }
    else
    {
      const T mult = 1. / frames;
      for(int i = 0; i < frames; ++i)
      {
        out[i] = mult * in[i];
      }
    }
  }
};

}
