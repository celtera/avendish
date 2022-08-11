#pragma once

#include <boost/math/constants/constants.hpp>
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <fmt/format.h>

#include <span>

namespace chowdsp_ports
{
using namespace chowdsp::wdft;

class PassiveLPF
{
public:
  // Static metadatas for the object
  static consteval auto name() { return "Passive LPF"; }
  static consteval auto c_name() { return "wdft_PassiveLPF"; }
  static consteval auto category() { return "Audio"; }
  static consteval auto author() { return "Jatin Chowdury"; }
  static consteval auto description() { return "Passive LPF"; }
  static consteval auto uuid() { return "bf92645a-b3d4-48ed-9dd6-8942aaf07e58"; }

  // Define the input ports
  struct inputs
  {
    struct
    {
      static consteval auto name() { return "fc"; }
      static consteval auto description() { return "Cutoff [Hz]"; }
      enum widget
      {
        hslider
      };
      struct range
      {
        float min = 20.f, max = 20'000.f, init = 1'000.f;
      };

      float value{};

      static void display(std::span<char> buffer, float value)
      {
        if(value <= 1000.0f)
          fmt::format_to(buffer.begin(), "{:.2f} Hz", value);
        else
          fmt::format_to(buffer.begin(), "{:.2f} kHz", value / 1000.f);
      }

      void update(PassiveLPF& obj)
      {
        const float cutoff = value;
        const auto Cap = 5.0e-8f;
        const auto Res = 1.0f / (boost::math::float_constants::two_pi * cutoff * Cap);

        obj.R1.setResistanceValue(Res);
        obj.R2.setResistanceValue(Res);
        obj.C1.setCapacitanceValue(Cap);
        obj.C2.setCapacitanceValue(Cap);
      }
    } cutoff;
  };

  // The helper library has a pre-made type for this, but we can just
  // define one inline
  struct setup
  {
    float rate;
  };
  void prepare(setup s)
  {
    C1.prepare(s.rate);
    C2.prepare(s.rate);
  }

  void reset()
  {
    C1.reset();
    C2.reset();
  }

  float operator()(float x)
  {
    Vs.setVoltage(x);

    Vs.incident(S2.reflected());
    auto y = voltage<float>(C2);
    S2.incident(Vs.reflected());

    return y;
  }

private:
  ResistorT<float> R1{1000.0f};
  ResistorT<float> R2{1000.0f};

  CapacitorT<float> C1{5.0e-8f, 48000.0f};
  CapacitorT<float> C2{5.0e-8f, 48000.0f};

  WDFSeriesT<float, decltype(R2), decltype(C2)> S1{R2, C2};
  WDFParallelT<float, decltype(C1), decltype(S1)> P1{C1, S1};
  WDFSeriesT<float, decltype(R1), decltype(P1)> S2{R1, P1};

  IdealVoltageSourceT<float, decltype(S2)> Vs{S2};
};
}
