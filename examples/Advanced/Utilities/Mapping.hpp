#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

#include <variant>

namespace ao
{

static constexpr halp::range_slider_range mapping_range{
    .min = -1e5, .max = 1e5, .init = {0, 1}};

// Q. : parametrizable but keep the metadata the same?
// Ok pour dataflow fixe, pas ok pour dataflow à la ossia::value
//
// Q. : shared state vs dynamic state?
// Q. : instantiate things how in the e.g. "ossia::value" case?

struct Mapping;

template <typename T>
struct M_Base
{
  Mapping& self;
  explicit M_Base(Mapping& x)
      : self{x}
  {
  }

  struct
  {
    halp::val_port<"In", T> value;
    halp::curve_port<"Curve"> curve;

    halp::range_spinbox_f32<"In range", mapping_range> xrange;
    halp::range_spinbox_f32<"Out range", mapping_range> yrange;
  } inputs;

  struct
  {
    halp::val_port<"Out", T> value;
  } outputs;
};

template <typename T>
  requires(std::integral<T> || std::floating_point<T>)
struct M : M_Base<T>
{
  using M_Base<T>::M_Base;

  void operator()() noexcept
  {
    const auto [x0, x1] = this->inputs.xrange.value;
    const auto [y0, y1] = this->inputs.yrange.value;
    // 1. value from its range to 0-1
    const double v = (this->inputs.value - x0) / (x1 - x0);

    // 2. map
    const double m = this->inputs.curve.value.value_at(v);

    // 3. apply the output range
    const double o = m * (y1 - y0) + y0;

    this->outputs.value = o;
  }
};

template <>
struct M<bool> : M_Base<bool>
{
  using M_Base<bool>::M_Base;

  void operator()() noexcept
  {
    this->outputs.value
        = this->inputs.curve.value.value_at((bool)this->inputs.value) >= 0.5;
  }
};

struct Mapping : M<float>
{
public:
  halp_meta(name, "Mapping")
  halp_meta(c_name, "mapping")
  halp_meta(category, "Mappings")
  halp_meta(description, "Mapping curve")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "3fa0eb4b-f280-420a-9674-c2696ddf17ff")

  Mapping()
      : M<float>{*this}
  {
  }

  template <typename T>
  using parametrization = M<T>;
};

struct value;
using value_v = std::variant<int, float, bool, std::string, std::vector<value>>;
struct value : value_v
{
  using value_v::value_v;
};

template <typename O, typename T>
concept parametrizable
    = std::is_constructible_v<typename O::template parametrization<T>, O&>;

template <typename T>
  requires parametrizable<Mapping, T> bool
instantiate()
{
  Mapping u;
  Mapping::parametrization<T> map{u};
  return true;
}

void instantiate_t(value x)
{
  std::visit(
      []<typename T>(T x) {
    if constexpr(requires { instantiate<T>(); })
      return instantiate<T>();
    return false;
      },
      x);
}

auto fun = [] {
  instantiate_t(int{});
  instantiate_t(float{});
  instantiate_t(std::string{});
  instantiate_t(bool{});
  instantiate_t(std::vector<value>{});
  return 0;
}();
}
