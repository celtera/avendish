#pragma once
#if __has_include(<smallfun_trivial.hpp>)
#include <smallfun_trivial.hpp>
#else
#include <functional>
#endif

namespace halp
{

struct power_curve_segment
{
  struct
  {
    float x{}, y{};
  } start;

  struct
  {
    float x{}, y{};
  } end;

  float gamma{1.0};
};

struct power_curve : std::vector<power_curve_segment>
{
  float value_at(float position)
  {
    // 1. Find the segment we are in
    if(this->empty())
      return 0.;

    auto it = std::upper_bound(
        this->begin(), this->end(), position,
        [](float v, const value_type& segt) noexcept { return v < segt.start.x; });

    if(it == this->end())
    {
      auto& segment = this->back();
      return segment.end.y; // FIXME with a pow function?
    }

    if(it != this->begin())
      --it;

    auto& segment = *it;
    if(segment.end.x - segment.start.x < 1e-8f)
      return segment.start.y; // FIXME with a pow function?

    // 2. Rescale global position to position in the segment.
    // spos is in [0; 1].
    const float spos = (position - segment.start.x) / (segment.end.x - segment.start.x);

    // 3. Apply the mapping function (in [0; 1] -> [0; 1])
    const float smap = std::pow(spos, segment.gamma);

    // 4. Scale the result into the y axis
    const float sval = segment.start.y + smap * (segment.end.y - segment.start.y);

    return sval;
  }
};

struct custom_curve_segment
{
  float start{};
  float end{};

#if __has_include(<smallfun_trivial.hpp>)
  using curve_function_t
      = smallfun::trivial_function<float(float), 3 * sizeof(float), alignof(float)>;
#else
  using curve_function_t = std::function<float(float)>;
#endif

  curve_function_t function{};
};

struct custom_curve : std::vector<custom_curve_segment>
{
  float value_at(float position)
  {
    // 1. Find the segment we are in
    if(this->empty())
      return 0.;

    auto it = std::upper_bound(
        this->begin(), this->end(), position,
        [](float v, const value_type& segt) noexcept { return v < segt.start; });

    if(it == this->end())
    {
      auto& segment = this->back();
      return segment.function(segment.end);
    }

    if(it != this->begin())
    {
      --it;
    }

    auto& segment = *it;
    if(segment.end - segment.start < 1e-8f)
      return segment.function(segment.start);

    // 2. Rescale global position to position in the segment.
    // spos is in [0; 1].
    const float spos = (position - segment.start) / (segment.end - segment.start);

    // 2. Apply the mapping function (in [0; 1] -> [min; max])
    return segment.function(spos);
  }
};

template <static_string lit, typename Curve = halp::custom_curve>
struct curve_port
{
  enum widget
  {
    curve
  };
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  Curve value;
  operator Curve&() noexcept { return value; }
  operator const Curve&() const noexcept { return value; }
  auto& operator=(Curve&& t) noexcept
  {
    value = std::move(t);
    return *this;
  }
  auto& operator=(const Curve& t) noexcept
  {
    value = t;
    return *this;
  }
};
}
