#pragma once
#include <halp/audio.hpp>
#include <halp/mappers.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

namespace mo
{
namespace
{
template <typename T>
static void duplicate_vector(T& vec)
{
  const int N = vec.size();
  vec.reserve(N * 2);
  for(int i = 0; i < N; i++)
    vec.push_back(vec[i]);
}

struct ArpeggioControl
{
  halp_meta(name, "Arpeggio")
  enum Arpeggio
  {
    Forward,
    Backward,
    FB,
    BF,
    Chord
  } value;
  enum widget
  {
    enumeration,
    list,
    combobox
  };
  struct range
  {
    std::string_view values[5]{"Forward", "Backward", "F -> B", "B -> F", "Chord"};
    Arpeggio init = Forward;
  };
  constexpr operator Arpeggio&() noexcept { return value; }
  constexpr operator const Arpeggio&() const noexcept { return value; }
  auto& operator=(Arpeggio other) noexcept
  {
    value = other;
    return *this;
  }
};
}
struct Arpeggiator
{
  halp_meta(name, "Arpeggiator")
  halp_meta(c_name, "arpeggiator")
  halp_meta(category, "Midi");
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Basic MIDI arpeggiator")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/midi-utilities.html#arpeggiator")
  halp_meta(uuid, "0b98c7cd-f831-468f-81e3-706d6a97d706")

  struct
  {
    halp::midi_bus<"in", libremidi::message> in;
    ArpeggioControl arpeggio;
    halp::hslider_i32<"Octave", halp::range{1, 7, 1}> octave;
    // halp::hslider_i32<"Quantification", halp::range{1, 32, 8}> quantif;

    struct : halp::time_chooser<"Quantification", halp::range{0.001, 30., 0.2}>
    {
      using mapper = halp::log_mapper<std::ratio<95, 100>>;
      //using mapper = halp::log_mapper<std::ratio<95, 100>>;
    } quantif;
  } inputs;

  struct
  {
    halp::midi_bus<"out", libremidi::message> out;
  } outputs;

  using byte = unsigned char;
  using chord = ossia::small_vector<std::pair<byte, byte>, 5>;

  ossia::flat_map<byte, byte> notes;
  ossia::small_vector<chord, 10> arpeggio;
  std::array<int8_t, 128> in_flight{};

  float previous_octave{-1};
  ArpeggioControl::Arpeggio previous_arpeggio{(ArpeggioControl::Arpeggio)-1};
  std::size_t index{};

  void update()
  {
    // Create the content of the arpeggio
    switch(previous_arpeggio)
    {
      case 0:
        arpeggiate(1);
        break;
      case 1:
        arpeggiate(1);
        std::reverse(arpeggio.begin(), arpeggio.end());
        break;
      case 2:
        arpeggiate(2);
        duplicate_vector(arpeggio);
        std::reverse(arpeggio.begin() + notes.size(), arpeggio.end());
        break;
      case 3:
        arpeggiate(2);
        duplicate_vector(arpeggio);
        std::reverse(arpeggio.begin(), arpeggio.begin() + notes.size());
        break;
      case 4:
        arpeggio.clear();
        arpeggio.resize(1);
        for(std::pair note : notes)
        {
          arpeggio[0].push_back(note);
        }
        break;
    }

    const std::size_t orig_size = arpeggio.size();

    // Create the octave duplicates
    for(int i = 1; i < previous_octave; i++)
    {
      octavize(orig_size, i);
    }
    for(int i = 1; i < previous_octave; i++)
    {
      octavize(orig_size, -i);
    }
  }

  void arpeggiate(int size_mult)
  {
    arpeggio.clear();
    arpeggio.reserve(notes.size() * size_mult);
    for(std::pair note : notes)
    {
      arpeggio.push_back(chord{note});
    }
  }

  void octavize(std::size_t orig_size, int i)
  {
    for(std::size_t j = 0; j < orig_size; j++)
    {
      auto copy = arpeggio[j];
      for(auto it = copy.begin(); it != copy.end();)
      {
        auto& note = *it;
        int res = note.first + 12 * i;
        if(res >= 0.f && res <= 127.f)
        {
          note.first = res;
          ++it;
        }
        else
        {
          it = copy.erase(it);
        }
      }

      arpeggio.push_back(std::move(copy));
    }
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical tk)
  {
    // Store the current chord in a buffer
    auto msgs = inputs.in.midi_messages;
    int octave = inputs.octave;
    ArpeggioControl::Arpeggio arpeggio = inputs.arpeggio;

    if(msgs.size() > 0)
    {
      // Update the "running" notes
      for(auto& note : msgs)
      {
        if(note.get_message_type() == libremidi::message_type::NOTE_ON)
        {
          this->notes.insert({note.bytes[1], note.bytes[2]});
        }
        else if(note.get_message_type() == libremidi::message_type::NOTE_OFF)
        {
          this->notes.erase(note.bytes[1]);
        }
      }
    }

    // Update the arpeggio itself
    const bool mustUpdateArpeggio = msgs.size() > 0 || octave != this->previous_octave
                                    || arpeggio != this->previous_arpeggio;
    if(mustUpdateArpeggio)
    {
      this->previous_octave = octave;
      this->previous_arpeggio = arpeggio;

      qDebug() << octave << (int)arpeggio;
      this->update();
    }

    if(this->arpeggio.empty())
    {
      for(int k = 0; k < 128; k++)
      {
        while(this->in_flight[k] > 0)
        {
          auto msg = libremidi::message::note_off(1, k, 0);
          msg.timestamp = 0;
          outputs.out.push_back(msg);
          this->in_flight[k]--;
        }
      }
      return;
    }

    if(this->index >= this->arpeggio.size())
      this->index = 0;

    // Play the next note / chord if we're on a quantification marker
    // !!! FIXME: find why it's different than the call in Arpeggiator
    for(auto [pos, q] : tk.get_quantification_date(inputs.quantif * 2.))
    {
      if(pos >= tk.frames)
        continue;
      // Finish previous notes

      for(int k = 0; k < 128; k++)
      {
        while(this->in_flight[k] > 0)
        {
          auto msg = libremidi::message::note_off(1, k, 0);
          msg.timestamp = pos;
          outputs.out.push_back(msg);
          this->in_flight[k]--;
        }
      }

      // Start the next note in the chord
      auto& chord = this->arpeggio[this->index];

      for(auto& note : chord)
      {
        this->in_flight[note.first]++;

        auto msg = libremidi::message::note_on(1, note.first, note.second);
        msg.timestamp = pos;
        outputs.out.push_back(msg);
      }

      // New chord to stop
      this->index = (this->index + 1) % (this->arpeggio.size());
    }
  }
};
}
