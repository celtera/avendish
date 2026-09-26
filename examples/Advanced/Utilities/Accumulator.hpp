#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/kurtosis.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>

#include <array>
#include <memory>
#include <optional>

namespace ao
{
namespace ba = boost::accumulators;
namespace bt = ba::tag;
/**
 * @brief Calibrate a value and output it between 0-1 according to
 * the range of inputs
 */
struct Accumulator
{
public:
  halp_meta(name, "Accumulator")
  halp_meta(c_name, "accumulator")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Accumulate statistics about incoming values")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/accumulator.html")
  halp_meta(uuid, "5c5b37b5-da06-432a-bc51-81657b6d59e1")

  enum OutputMode
  {
    EveryTick,
    OnInput,
    Manually
  };

  struct inputs_t
  {
    halp::val_port<"In", std::optional<float>> in;
    struct : halp::toggle<"Reset">
    {
      void update(Accumulator& self) { self.reset(); }
    } reset;
    struct : halp::impulse_button<"Output">
    {
      void update(Accumulator& self) { self.bang = true; }
    } output;
    //! Every tick (the default), on each input, or only on the Output bang.
    halp::combobox_t<"Send", OutputMode> when;
  } inputs;

  struct
  {
    struct : halp::val_port<"Sum", std::optional<float>>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } sum;
    struct : halp::val_port<"Count", std::optional<float>>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } count;
    struct : halp::val_port<"Consecutive difference", std::optional<float>>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } diff;
    struct : halp::val_port<"Mean", std::optional<float>>
    {
      struct range
      {
        const float min = 1.f;
        const float max = 0.f;
        const float init = 0.f;
      };
    } mean;
    struct : halp::val_port<"Variance", std::optional<float>>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } variance;
    struct : halp::val_port<"Median", std::optional<float>>
    {
      struct range
      {
        const float min = 1.f;
        const float max = 0.f;
        const float init = 0.f;
      };
    } median;
    struct : halp::val_port<"Kurtosis", std::optional<float>>
    {
      struct range
      {
        const float min = 1.f;
        const float max = 0.f;
        const float init = 0.f;
      };
    } kurtosis;

    struct : halp::val_port<"Min", std::optional<float>>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } min;
    struct : halp::val_port<"Max", std::optional<float>>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } max;

  } outputs;

  using accum = ba::accumulator_set<
      float, ba::stats<
                 ba::tag::count, ba::tag::sum, ba::tag::min, ba::tag::max, ba::tag::mean,
                 ba::tag::variance, ba::tag::median, ba::tag::kurtosis>>;

  accum minmax{};
  float consecutive_difference{};
  bool consecutive_difference_sign{};

  bool bang{};
  // A reset is news: in "on input" mode it sends the zeros once.
  bool reset_pending{};

  void reset()
  {
    std::destroy_at(&minmax);
    std::construct_at(&minmax);
    consecutive_difference = 0.f;
    consecutive_difference_sign = false;
    reset_pending = true;
  }

  std::array<std::optional<float>*, 9> all_outputs() noexcept
  {
    return {&outputs.count.value,    &outputs.sum.value,    &outputs.diff.value,
            &outputs.min.value,      &outputs.max.value,    &outputs.mean.value,
            &outputs.variance.value, &outputs.median.value, &outputs.kurtosis.value};
  }

  void send()
  {
    // Nothing accumulated (start, reset): zeros rather than the NaNs some
    // statistics of an empty set give.
    if(ba::extract::count(minmax) == 0)
    {
      for(auto* p : all_outputs())
        *p = 0.f;
      return;
    }
    outputs.count.value = ba::extract::count(minmax);
    outputs.sum.value = ba::extract::sum(minmax);
    outputs.diff.value = consecutive_difference;
    outputs.min.value = ba::extract::min(minmax);
    outputs.max.value = ba::extract::max(minmax);
    outputs.mean.value = ba::extract::mean(minmax);
    outputs.variance.value = ba::extract::variance(minmax);
    outputs.median.value = ba::extract::median(minmax);
    outputs.kurtosis.value = ba::extract::kurtosis(minmax);
  }

  void operator()() noexcept
  {
    const bool input = bool(inputs.in.value);
    if(input)
    {
      float v = *inputs.in.value;
      this->minmax(v);
      if(consecutive_difference_sign ^= true)
        consecutive_difference += v;
      else
        consecutive_difference -= v;
    }

    bool out{};
    switch(inputs.when)
    {
      default:
      case EveryTick:
        out = true;
        break;
      case OnInput:
        out = input || reset_pending;
        break;
      case Manually:
        out = bang;
        break;
    }
    if(out)
      send();
    else
      for(auto* p : all_outputs())
        p->reset();
    reset_pending = false;
    bang = false;
  }


};

}
