#pragma once
#include <cinttypes>
#include <cstdint>
#include <string_view>

namespace halp
{

struct dummy_range
{
};

struct impulse
{
};

struct impulse_type
{
};

/// Basic data types
template <typename T>
struct xy_type
{
  T x, y;

  constexpr xy_type& operator=(T single) noexcept
  {
    x = single;
    y = single;
    return *this;
  }
};

template <typename T>
struct xyz_type
{
  T x, y, z;

  constexpr xyz_type& operator=(T single) noexcept
  {
    x = single;
    y = single;
    z = single;
    return *this;
  }
};

struct color_type
{
  float r, g, b, a;
  constexpr color_type& operator=(float single) noexcept
  {
    r = single;
    g = single;
    b = single;
    a = single;
    return *this;
  }
};

struct r_color
{
  uint8_t r;
};

struct rgba_color
{
  uint8_t r, g, b, a;
};

struct rgba32f_color
{
  float r, g, b, a;
};

struct rgb_color
{
  uint8_t r, g, b;
};

template <typename T>
struct range_slider_value
{
  T start, end;
};

// { { "foo", 1.5 },  { "bar", 4.0 } }
template <typename T>
struct combo_pair
{
  std::string_view first;
  T second;
};

struct ratio
{
  int num{1}, denom{1};
};

}
