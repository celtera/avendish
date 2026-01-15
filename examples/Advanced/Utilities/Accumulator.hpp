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

  struct inputs_t
  {
    halp::val_port<"In", std::optional<float>> in;
    struct : halp::toggle<"Reset">
    {
      void update(Accumulator& self)
      {
        std::destroy_at(&self.minmax);
        std::construct_at(&self.minmax);
        self.outputs.count.value = 0;
        self.outputs.sum.value = 0;
        self.outputs.diff.value = 0;
        self.outputs.min.value = 0;
        self.outputs.max.value = 0;
        self.outputs.mean.value = 0;
        self.outputs.variance.value = 0;
        self.outputs.median.value = 0;
        self.outputs.kurtosis.value = 0;
      }
    } reset;
  } inputs;

  struct
  {
    struct : halp::val_port<"Sum", float>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } sum;
    struct : halp::val_port<"Count", float>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } count;
    struct : halp::val_port<"Consecutive difference", float>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } diff;
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
    struct : halp::val_port<"Median", float>
    {
      struct range
      {
        const float min = 1.f;
        const float max = 0.f;
        const float init = 0.f;
      };
    } median;
    struct : halp::val_port<"Kurtosis", float>
    {
      struct range
      {
        const float min = 1.f;
        const float max = 0.f;
        const float init = 0.f;
      };
    } kurtosis;

    struct : halp::val_port<"Min", float>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 1.f;
        const float init = 0.f;
      };
    } min;
    struct : halp::val_port<"Max", float>
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

  void operator()() noexcept
  {
    using namespace ba;
    if(inputs.in.value)
    {
      float v = *inputs.in.value;
      this->minmax(v);
      if(consecutive_difference_sign ^= true)
        consecutive_difference += v;
      else
        consecutive_difference -= v;

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
  }
};

}
