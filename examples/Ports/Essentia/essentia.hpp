#pragma once
#include <halp/static_string.hpp>

namespace essentia_ports
{
using Real = double;
//
template <halp::static_string Name, halp::static_string Desc>
struct array_port
{
  static consteval auto name() noexcept { return Name; }
  static consteval auto description() noexcept { return Desc; }

  Real* channel{};

  operator Real*() const noexcept { return channel; }
};
template <halp::static_string Name, halp::static_string Desc>
struct real_port
{
  static consteval auto name() noexcept { return Name; }
  static consteval auto description() noexcept { return Desc; }

  Real value{};

  operator Real() const noexcept { return value; }
  operator Real&() noexcept { return value; }
};
}

#define DOC(TEXT) TEXT
