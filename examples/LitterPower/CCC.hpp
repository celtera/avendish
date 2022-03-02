#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>

namespace examples
{
/**
 * As an example, here is a port of the CCC chaotic engine part of
 * Litter Power, the traditional set of Max/MSP externals by Peter Castine.
 */
struct CCC
{
  $(name, "CCC");
  $(script_name, "CCC");
  $(category, "Demo");
  $(author, "Peter Castine");
  $(description, "1/f noise, using the Schuster/Procaccia deterministic (chaotic) algorithm");
  $(uuid, "9db0af3c-8573-4541-95d4-cf7902cdbedb");

  struct {
    /**
     * Here we use a bang input like the original Max external ;
     * notice that an impulse value as-is wouldn't make a lot of sense.
      **/
    avnd::accurate<avnd::val_port<"Bang", avnd::impulse>> bang;
  } inputs;

  struct {
    /** One float is output per bang **/
    avnd::accurate<avnd::val_port<"Out", float>> out;
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
        if (curVal <= kMinPink) {
          if (curVal == 0.0)	curVal  = kMinPink;
          else				curVal += curVal;
        }

        curVal = curVal * curVal + curVal;
        if (curVal >= 1.0)					// Cheaper than fmod(), and works quite nicely
          curVal -= 1.0;					// in the range of values that can occur.

        this->current_value = curVal;
      }

      outputs.out.values[timestamp] = current_value;
    }
  }
};
}
