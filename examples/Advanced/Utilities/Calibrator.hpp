#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/rolling_variance.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>

namespace ao
{
namespace ba = boost::accumulators;
namespace bt = ba::tag;
/**
 * @brief Calibrate a value and output it between 0-1 according to
 * the range of inputs
 */
struct Calibrator
{
public:
  halp_meta(name, "Calibrator")
  halp_meta(c_name, "calibrator")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Calibrate a value with unknown range")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/calibrator.html")
  halp_meta(uuid, "b9187d76-207c-4e28-b603-9d88d927f59d")

  struct inputs_t
  {
    halp::val_port<"In", std::optional<float>> in;
    halp::range_slider_f32<"Range", halp::range_slider_range{0., 1., {0.1, 0.9}}> range;

    struct
    {
      halp__enum("Mode", Clip, Free, Clip, Shape, Wrap, Fold);
    } wrap;

    struct : halp::hslider_i32<"Average over...", halp::range{1., 10000., 100.}>
    {
      using mapper = halp::log_mapper<std::ratio<95, 100>>;
      void update(Calibrator& self)
      {
        std::destroy_at(&self.stat);
        std::construct_at(&self.stat, ba::tag::rolling_window::window_size = value);
      }
    } window;

    struct : halp::impulse_button<"Reset">
    {
      void update(Calibrator& self)
      {
        std::destroy_at(&self.minmax);
        std::construct_at(&self.minmax);
        std::destroy_at(&self.stat);
        std::construct_at(
            &self.stat, ba::tag::rolling_window::window_size = self.inputs.window.value);
      }
    } reset;

  } inputs;

  struct
  {
    struct : halp::val_port<"Scaled", float>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } scaled;
    struct : halp::val_port<"Mean", float>
    {
      struct range
      {
        const float min = 1.f;
        const float max = 0.f;
        const float init = 0.f;
      };
    } mean;
    struct : halp::val_port<"Variance", float>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } variance;

    halp::hslider_f32<"Cur min", halp::range{-10., 10., 0.}> min;
    halp::hslider_f32<"Cur max", halp::range{-10., 10., 0.}> max;
  } outputs;

  using minmax_accum = ba::accumulator_set<float, ba::stats<ba::tag::min, ba::tag::max>>;
  using stat_accum = ba::accumulator_set<float, ba::stats<ba::tag::rolling_variance>>;

  minmax_accum minmax{};

  static constexpr float rescale(float v, float min, float max) noexcept
  {
    return (v - min) / (max - min);
  }

  float wrap(float f)
  {
    using mode_t = decltype(inputs.wrap)::enum_type;
    switch(inputs.wrap.value)
    {
      case mode_t::Free:
        return f;
      default:
      case mode_t::Clip:
        return ossia::clamp<float>(f, 0.f, 1.f);
      case mode_t::Shape:
        return (std::tanh(f * 2.f - 1.f) + 1.f) / 2.f;
      case mode_t::Wrap:
        return ossia::wrap<float>(f, 0.f, 1.f);
      case mode_t::Fold:
        return ossia::fold<float>(f, 0.f, 1.f);
    }
  }

  stat_accum stat{ba::tag::rolling_window::window_size = 500};
  void operator()() noexcept
  {
    using namespace ba;
    if(inputs.in.value)
    {
      float v = *inputs.in.value;
      this->minmax(v);

      outputs.min.value = ba::extract::min(minmax);
      outputs.max.value = ba::extract::max(minmax);
      float min = outputs.min.value;
      float max = outputs.max.value;

      if(BOOST_UNLIKELY(std::abs(min - max) < 1e-6))
      {
        outputs.scaled.value = 0.5f;
      }
      else
      {
        if(BOOST_UNLIKELY(min > max))
          std::exchange(min, max);

        auto [umin, umax] = inputs.range.value;
        if(BOOST_UNLIKELY(std::abs(umin - umax) < 1e-6))
        {
          umin = min;
          umax = max;
        }
        else if(BOOST_UNLIKELY(umin > umax))
          std::exchange(umin, umax);

        // // Put the value between [0;1]
        // outputs.scaled.value = rescale(v, min, max);
        //
        // // Thenescale it with the user ratio
        // outputs.scaled.value = rescale(outputs.scaled.value, umin, umax);

        // Let's try to skip a division instead:
        const float range = (max - min);
        outputs.scaled.value = wrap((v - min - umin * range) / ((umax - umin) * range));
      }

      this->stat(outputs.scaled.value);

      outputs.mean.value = ba::extract::rolling_mean(stat);
      // maximum variance of a variable between [0; 1] is 1/4
      outputs.variance.value = ba::extract::rolling_variance(stat) * 4.;
    }
  }
};

}
