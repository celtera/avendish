#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <gpp/meta.hpp>
#include <halp/static_string.hpp>

namespace gpp
{

/// Quick way to define the pipeline layout
#define gpp_attribute(loc, n, type, ...) \
  struct                                 \
  {                                      \
    halp_meta(name, #n);                 \
    halp_flag(__VA_ARGS__);              \
    static constexpr int location()      \
    {                                    \
      return loc;                        \
    }                                    \
    using data_type = type;              \
    data_type data;                      \
  }

#define gpp_compute(loc, buffer_n, n, type, ...) \
  struct                                         \
  {                                              \
    halp_meta(name, #n);                         \
    halp_meta(buffer_name, #buffer_n);           \
    halp_flag(__VA_ARGS__);                      \
    static constexpr int location()              \
    {                                            \
      return loc;                                \
    }                                            \
    using data_type = type;                      \
    data_type data;                              \
  }

// And some common types predefined for the most common use cases
struct vertex_position_out
{
  halp_meta(name, "gl_Position");
  halp_flag(per_vertex);
  float data[4];
};

// To be used in the layout bindings
template <halp::static_string lit, int bnd>
struct sampler
{
  static constexpr std::string_view name() { return lit.value; }
  halp_flag(sampler2D);
  static constexpr int binding() { return bnd; }
};

template <halp::static_string lit, int bnd>
struct image
{
  static constexpr std::string_view name() { return lit.value; }
  halp_flags(image2D, load, store);
  static constexpr int binding() { return bnd; }
};

template <halp::static_string lit, int bnd>
struct image_input
{
  static constexpr std::string_view name() { return lit.value; }
  halp_flags(image2D, readonly);
  static constexpr int binding() { return bnd; }
};

template <halp::static_string lit, int bnd>
struct image_output
{
  static constexpr std::string_view name() { return lit.value; }
  halp_flags(image2D, writeonly);
  static constexpr int binding() { return bnd; }
};

template <halp::static_string lit, typename T>
struct uniform
{
  static constexpr std::string_view name() { return lit.value; }
  T value;
};

// Those are to be used as the object ports
template <halp::static_string lit, auto T>
struct texture_input_port
{
  static constexpr std::string_view name() { return lit.value; }
  static constexpr auto sampler() { return T; }
};

template <halp::static_string lit, auto T>
struct image_input_port
{
  static constexpr std::string_view name() { return lit.value; }
  static constexpr auto image() { return T; }
};

template <halp::static_string lit, auto T>
struct color_attachment_port
{
  static constexpr std::string_view name() { return lit.value; }
  static constexpr auto attachment() { return T; }
};

template <typename Widget, auto T>
struct uniform_control_port : Widget
{
  static constexpr auto uniform() { return T; }
};

}
