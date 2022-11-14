#pragma once
#include <boost/container/small_vector.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <halp/audio.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
namespace melodial
{
struct Note
{
  int pitch;
  int velocity;
};

using Chord = boost::container::small_vector<Note, 4>;
using Pattern = boost::container::small_vector<Chord, 32>;

struct Processor
{
  halp_meta(name, "Melodial")
  halp_meta(c_name, "melodial")
  halp_meta(category, "Midi")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(
      description,
      "Convert a melodic pattern into MIDI notes. \n"
      "Pattern data format: [\n"
      "  [ [ 48, 60 ], [ 55, 127 ] ], // C3 G3 chord\n"
      "  [ [ 48, 127 ] ] // single C3, louder \n"
      "]\n")
  halp_meta(uuid, "5b34abbb-dcd2-47d4-a836-f36c17d306ef")
  struct
  {
    struct
    {
      Pattern value;
    } pattern;
  } inputs;

  struct
  {
    halp::midi_bus<"Out"> midi;
  } outputs;

  using tick = halp::tick_musical;
  std::vector<int> running;
  void operator()(halp::tick_musical tk)
  {
    if(inputs.pattern.value.empty())
    {
      // FIXME send note off
      return;
    }

    const int pattern_length = inputs.pattern.value.size();

    auto& pat = inputs.pattern.value;

    //for(auto& [note, vel] : inputs.patterns.value)
    {
      {
        auto quants = tk.get_quantification_date(4. / pattern_length);
        for(auto [pos, q] : quants)
        {
          if(pos < tk.frames)
          {
            // Stop running notes
            for(uint8_t n : running)
            {
              halp::midi_msg m;
              m.bytes = {128, n, 0};
              m.timestamp = pos;
              outputs.midi.midi_messages.push_back(m);
            }
            running.clear();

            // Start new notes
            auto qq = std::abs(q % pattern_length);
            //if(pat[qq].velocity > 0)
            for(auto [pitch, vel] : pat[qq])
            {
              halp::midi_msg m;
              m.bytes = {144, (uint8_t)pitch, (uint8_t)vel};
              m.timestamp = pos;
              outputs.midi.midi_messages.push_back(m);
              running.push_back(pitch);
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
