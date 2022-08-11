#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

namespace litterpower_ports
{
/**
 * As an example, here is a port of the CCC chaotic engine part of
 * Litter Power, the traditional set of Max/MSP externals by Peter Castine.
 */
struct CCC
{
  halp_meta(name, "CCC");
  halp_meta(c_name, "avnd_lp_ccc");
  halp_meta(category, "Demo");
  halp_meta(author, "Peter Castine");
  halp_meta(
      description,
      "1/f noise, using the Schuster/Procaccia deterministic (chaotic) algorithm");
  halp_meta(uuid, "9db0af3c-8573-4541-95d4-cf7902cdbedb");

  struct
  {
    /**
     * Here we use a bang input like the original Max external ;
     * notice that an impulse value as-is wouldn't make a lot of sense.
      **/
    halp::accurate<halp::val_port<"Bang", halp::impulse>> bang;
  } inputs;

  struct
  {
    /** One float is output per bang **/
    halp::accurate<halp::val_port<"Out", float>> out;
  } outputs;

  /** We need to keep some state around **/
  double current_value{0.1234};

  /** No particular argument is needed here, we can just process the whole input buffer **/
  void operator()()
  {
    for(auto& [timestamp, value] : inputs.bang.values)
    {
      // CCC algorithm, copied verbatim from the LitterPower source code.
      {
        constexpr double kMinPink = 1.0 / 525288.0;

        double curVal = this->current_value;

        // Sanity check... due to limitations in accuracy, we can die at very small values.
        // Also, we prefer to only "nudge" the value towards chaos...
        if(curVal <= kMinPink)
        {
          if(curVal == 0.0)
            curVal = kMinPink;
          else
            curVal += curVal;
        }

        curVal = curVal * curVal + curVal;
        if(curVal >= 1.0) // Cheaper than fmod(), and works quite nicely
          curVal -= 1.0;  // in the range of values that can occur.

        this->current_value = curVal;
      }

      outputs.out.values[timestamp] = current_value;
    }
  }
};
}
