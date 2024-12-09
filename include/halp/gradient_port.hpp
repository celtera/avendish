#pragma once
#include <boost/container/flat_map.hpp>
#include <halp/modules.hpp>
#include <halp/static_string.hpp>
#include <halp/value_types.hpp>
HALP_MODULE_EXPORT
namespace halp
{
struct xyz_color
{
  double x{}, y{}, z{};
  rgba32f_color to_rgba()
  {
    auto var_X = x / 100.; // X from 0 to  95.047      (Observer = 2°, Illuminant = D65)
    auto var_Y = y / 100.; // Y from 0 to 100.000
    auto var_Z = z / 100.; // Z from 0 to 108.883

    auto var_R = var_X * 3.2406 + var_Y * -1.5372 + var_Z * -0.4986;
    auto var_G = var_X * -0.9689 + var_Y * 1.8758 + var_Z * 0.0415;
    auto var_B = var_X * 0.0557 + var_Y * -0.2040 + var_Z * 1.0570;

    auto translate = [](auto var) -> float {
      return std::clamp(
          static_cast<decltype(0.)>(
              var > 0.0031308 ? 1.055 * (std::pow(var, (1 / 2.4))) - 0.055
                              : var * 12.92),
          0., 1.);
    };

    return rgba32f_color{translate(var_R), translate(var_G), translate(var_B), 1.f};
  }
};

struct hunter_lab_color
{
  double l{}, a{}, b{};

  rgba32f_color to_rgba()
  {
    const auto x = (a / 17.5) * (l / 10.0);
    const auto y = l * l / 100.;
    const auto z = b / 7.0 * l / 10.0;

    xyz_color xyz{(float)((x + y) / 1.02), (float)y, (float)(-(z - y) / 0.847)};
    return xyz.to_rgba();
  }
};

struct gradient_t : boost::container::flat_map<float, hunter_lab_color>
{
  using flat_map::flat_map;

  struct ease
  {
    template <typename T, typename U, typename V>
    constexpr T operator()(T a, U b, V t) const noexcept
    {
#if defined(FP_FAST_FMA)
      return std::fma(t, b, std::fma(-t, a, a));
#else
      return (static_cast<T>(1) - t) * a + t * b;
#endif
    }
  };

  rgba32f_color ease_color(
      double prev_pos, hunter_lab_color prev, double next_pos, hunter_lab_color next,
      double pos)
  {
    // Interpolate in La*b* domain
    const auto coeff = (pos - prev_pos) / (next_pos - prev_pos);

    hunter_lab_color res;
    ease e{};
    res.l = e(prev.l, next.l, coeff);
    res.a = e(prev.a, next.a, coeff);
    res.b = e(prev.b, next.b, coeff);

    return res.to_rgba();
  }

  rgba32f_color value_at(float position)
  {
    auto& m_data = *this;
    switch(m_data.size())
    {
      case 0:
        return rgba32f_color{0., 0., 0., 1.};
      case 1:
        return m_data.begin()->second.to_rgba();
      default: {
        auto it_next = m_data.lower_bound(position);
        // Before start
        if(it_next == m_data.begin())
          return m_data.begin()->second.to_rgba();

        // past end
        else if(it_next == m_data.end())
          return m_data.rbegin()->second.to_rgba();
        else
        {
          auto it_prev = it_next;
          --it_prev;
          return ease_color(
              it_prev->first, it_prev->second, it_next->first, it_next->second,
              position);
        }
      }
    }
  }
};

template <static_string lit>
struct gradient_port
{
  enum widget
  {
    gradient
  };
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  gradient_t value;
  operator gradient_t&() noexcept { return value; }
  operator const gradient_t&() const noexcept { return value; }
  auto& operator=(gradient_t&& t) noexcept
  {
    value = std::move(t);
    return *this;
  }
  auto& operator=(const gradient_t& t) noexcept
  {
    value = t;
    return *this;
  }
};
}
