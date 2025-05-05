#pragma once
#include <boost/container/small_vector.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <halp/audio.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
namespace patternal
{

/**
 * A sequencer pattern.
 * Contains its note and its velocity on each beat.
 */
struct Pattern
{
  /**
   * A note.
   */
  int note;
  /**
   * Its velocity on each beat.
   */
  boost::container::small_vector<uint8_t, 32> pattern;
};

struct Processor
{
  halp_meta(name, "Patternal")
  halp_meta(c_name, "patternal")
  halp_meta(category, "Midi")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/patternal.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      description,
      "Convert a rhythmic pattern into MIDI notes. \n"
      "Pattern data format: [\n"
      "  [ 36, [ 127, 0, 0, 0 ] ], \n"
      "  [ 38, [ 0, 0, 64, 0 ] ], \n"
      "]\n"
      "for a very simple drum rythm on kick and snare.")
  halp_meta(uuid, "6e89b53a-1645-4a9c-a26e-e6c7870a902c")

  struct
  {
    struct
    {
      /**
       * A list of patterns to play.
       * Each note has a velocity that changes for each beat.
       */
      std::vector<Pattern> value;
    } patterns;
  } inputs;

  /**
   * Its MIDI output.
   */
  struct
  {
    halp::midi_bus<"Out"> midi;
  } outputs;

  void operator()(halp::tick_musical tk)
  {
    // Find out where we are in the bar
    // ... let's assume 4/4?

    for(auto& [note, pat] : inputs.patterns.value)
    {
      if(!pat.empty())
      {
        auto quants = tk.get_quantification_date(4. / pat.size());
        for(auto [pos, q] : quants)
        {
          // FIXME: The position returned by get_quantification_date is a negative timestamp.
          if(pos < tk.frames)
          {
            auto qq = std::abs(q % std::ssize(pat));
            if(uint8_t velocity = pat[qq]; velocity > 0)
            {
              halp::midi_msg m;
              // Note on:
              m.bytes = {144, (uint8_t)note, velocity};
              m.timestamp = pos;
              outputs.midi.midi_messages.push_back(m);

              // FIXME: The note off should not be output right away.
              // Note off:
              m.bytes = {128, (uint8_t)note, 0};
              m.timestamp = pos;
              outputs.midi.midi_messages.push_back(m);
            }
          }
          else
          {
            // qDebug() << pos << tk.frames;
          }
        }
      }
    }
  }
};
}
