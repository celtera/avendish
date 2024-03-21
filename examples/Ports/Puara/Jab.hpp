#pragma once
#include <boost/circular_buffer.hpp>
#include <halp/controls.hpp>
namespace puara
{
template <typename T>
struct rolling_min_max
{
  struct min_max
  {
    T min;
    T max;
  };

  min_max add(T value) noexcept
  {
    min_max ret{.min = value, .max = value};
    buf.push_back(value);
    for(const T value : buf)
    {
      if(value < ret.min)
        ret.min = value;
      if(value > ret.max)
        ret.max = value;
    }
    return ret;
  }

  boost::circular_buffer<T> buf = boost::circular_buffer<T>(10);
};

// Ported from:
// https://github.com/Puara/puara-gestures/blob/refactor/descriptors/puara-jab.cpp
struct Jab
{
  halp_meta(name, "Jab");
  halp_meta(c_name, "puara_jab");
  halp_meta(category, "Gesture analysis");
  halp_meta(author, "Eduardo Meneses");
  halp_meta(description, "Compute jab");
  halp_meta(uuid, "0b197ebb-e403-4aea-a005-f7422b1942d4");

  struct
  {
    halp::val_port<"Input", double> in;
    halp::knob_f32<"Threshold", halp::range{0.0001, 100., 5.}> threshold{};
  } inputs;
  struct
  {
    halp::val_port<"Output", double> out;
  } outputs;

  void operator()()
  {
    auto [min, max] = this->minmax.add(inputs.in.value);

    if(max - min > inputs.threshold)
    {
      if(max >= 0 && min >= 0)
        outputs.out = max - min;
      else if(max < 0 && min < 0)
        outputs.out = min - max;
      else
        outputs.out = max - min;
    }
  }

private:
  rolling_min_max<double> minmax;
};
}
