#pragma once
#include <boost/container/small_vector.hpp>
#include <cmath>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct DBAP_2d
{
  halp_meta(name, "DBAP (2D)")
  halp_meta(c_name, "avnd_dbap_2d")
  halp_meta(
      author,
      "Trond Lossius, Pia Baltazar, Théo de la Hogue, Matsuura Tomya, François Weber, "
      "Jean-Michaël Celerier")
  halp_meta(category, "Spatialization")
  halp_meta(description, "DBAP algorithm")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/dbap.html")
  halp_meta(uuid, "0679feac-2b80-4e23-bb36-25e3a37e4e7e")

  struct speaker
  {
    float x, y;
  };

  struct
  {
    halp::xy_spinboxes_f32<"Source"> src;
    halp::val_port<"Speakers", std::vector<speaker>> dst;
    halp::knob_f32<"Blur"> blur;
    halp::knob_f32<"Roll-off", halp::range{0.f, 100.f, 6.f}> rolloff;
  } inputs;

  struct
  {
    halp::val_port<"Output", boost::container::small_vector<float, 16>> out;
  } outputs;

  void operator()()
  {
    const auto N = inputs.dst.value.size();
    if(N == 0)
      return;
    auto& res = outputs.out.value;
    res.clear();
    res.resize(N, boost::container::default_init);

    const auto s = inputs.src.value;
    const auto blur = std::pow(inputs.blur, 2.f);
    const auto roff = inputs.rolloff.value * 0.166096f;

    float pow_sum = 0.f;
#pragma omp simd
    for(int i = 0; i < N; i++)
    {
      const auto [dx, dy] = inputs.dst.value[i];
      const float hypot
          = std::sqrt(std::pow(s.x - dx, 2.f) + std::pow(s.y - dy, 2.f) + blur)
            + 0.000001f;
      const float rolled_off = 1.f / std::pow(hypot, roff);
      res[i] = rolled_off;
      pow_sum += std::pow(rolled_off, 2.f);
    }

    pow_sum = 1.f / std::sqrt(pow_sum);
#pragma omp simd
    for(int i = 0; i < N; i++)
    {
      res[i] *= pow_sum;
    }
  }
};

struct DBAP_3d
{
  halp_meta(name, "DBAP (3D)")
  halp_meta(c_name, "avnd_dbap_3d")
  halp_meta(
      author,
      "Trond Lossius, Pia Baltazar, Théo de la Hogue, Matsuura Tomya, François Weber, "
      "Jean-Michaël Celerier")
  halp_meta(category, "Spatialization")
  halp_meta(description, "DBAP algorithm")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/dbap.html")
  halp_meta(uuid, "16baf7fa-89ed-4984-825b-be3eca5b879e")

  struct speaker
  {
    float x, y, z;
  };

  struct
  {
    halp::xyz_spinboxes_f32<"Source"> src;
    halp::val_port<"Speakers", std::vector<speaker>> dst;
    halp::knob_f32<"Blur"> blur;
    halp::knob_f32<"Roll-off", halp::range{0.f, 100.f, 6.f}> rolloff;
  } inputs;

  struct
  {
    halp::val_port<"Output", boost::container::small_vector<float, 16>> out;
  } outputs;

  void operator()()
  {
    const auto N = inputs.dst.value.size();
    if(N == 0)
      return;
    auto& res = outputs.out.value;
    res.clear();
    res.resize(N, boost::container::default_init);

    const auto s = inputs.src.value;
    const auto blur = std::pow(inputs.blur, 2.f);
    const auto roff = inputs.rolloff.value * 0.166096f;

    float pow_sum = 0.f;
#pragma omp simd
    for(int i = 0; i < N; i++)
    {
      const auto [dx, dy, dz] = inputs.dst.value[i];
      const float hypot = std::sqrt(
                              std::pow(s.x - dx, 2.f) + std::pow(s.y - dy, 2.f)
                              + std::pow(s.z - dz, 2.f) + blur)
                          + 0.000001f;
      const float rolled_off = 1.f / std::pow(hypot, roff);
      res[i] = rolled_off;
      pow_sum += std::pow(rolled_off, 2.f);
    }

    pow_sum = 1.f / std::sqrt(pow_sum);
#pragma omp simd
    for(int i = 0; i < N; i++)
    {
      res[i] *= pow_sum;
    }
  }
};
}
