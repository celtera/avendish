#pragma once

#include <chowdsp_wdf/chowdsp_wdf.h>

#include <halp/meta.hpp>

namespace chowdsp_ports
{
using namespace chowdsp::wdft;

// Example of defining a small helper class to define a control for a resistor
template<halp::static_string Name, halp::static_string Description, auto RMember>
struct ResistorControl {
  static consteval std::string_view name() { return Name.value; }
  static consteval std::string_view description() { return Description.value; }
  enum widget { knob };
  struct range {
    float min = 100.0f;
    float max = 1000000.0f;
    float init = 10000.0f;
    float skew = 10000.0f; // TODO
  };

  float value{};

  void update(auto& obj)
  {
    (obj.*RMember).setResistanceValue(value);
  }
};

class VoltageDivider
{
private:
    ResistorT<float> R1 { 10000.0f };
    ResistorT<float> R2 { 10000.0f };

    WDFSeriesT<float, decltype (R1), decltype (R2)> S1 { R1, R2 };
    PolarityInverterT<float, decltype (S1)> I1 { S1 };

    IdealVoltageSourceT<float, decltype (I1)> Vs { I1 };

public:
    // Saves a bit of typing vs. the "raw" way exercised in PassiveLPF
    halp_meta(name, "Voltage Divider")
    halp_meta(c_name, "wdft_VoltageDivider")
    halp_meta(category, "Audio")
    halp_meta(author, "Jatin Chowdury")
    halp_meta(description, "Voltage Divider")
    halp_meta(uuid, "806d297c-3339-49c6-ab5f-4678d04d4460")

    // Define the input ports
    struct inputs {
      ResistorControl<"r1", "R1 Value", &VoltageDivider::R1> r1;
      ResistorControl<"r2", "R2 Value", &VoltageDivider::R2> r2;
    };

    float operator()(float x)
    {
      Vs.setVoltage (x);

      Vs.incident (I1.reflected());
      I1.incident (Vs.reflected());

      return voltage<float> (R2);
    }
};
}
