# Midifle ports

Midifile ports are currently only supported with the ossia binding.

They allow to define an input which will be the content of a MIDI file.

## Port definition

A midifile input port assumes the existence of a MIDI event structure.

The whole thing can look like this:

```cpp
struct midi_event
{
  // The MIDI messages
  std::vector<unsigned char> bytes;

  // Can also be this, but meta events won't be available 
  // since they are > 3 bytes.
  unsigned char bytes[3];
  
  // optional, delta time in ticks
  int tick_delta;

  // optional, time since the beginning in ticks
  int tick_absolute; 
};

struct
{
  static consteval auto name() { return "My midifile"}
  struct {
    // Can be any vector-ish container, but must support push_back
    std::vector<std::vector<midi_event>> tracks; 

    // If present, the length in ticks 
    int64_t length{};

    // If present, the length in ticks     
    int64_t ticks_per_beat{};

    // If present, the length in ticks     
    float starting_tempo{};
    std::string_view filename; // Currently loaded midifile
  } midifile;
} snd;
```

A few helper types are provided:

```cpp
struct midi_track_event
{
  boost::container::small_vector<uint8_t, 15> bytes;
  int tick_delta = 0;
};

struct simple_midi_track_event
{
  uint8_t bytes[3];
  int64_t tick_absolute;
};

struct {
  // Default event type is simple_midi_track_event for performance and compile times, 
  // but it can be changed through the template arguments
  halp::midifile_port<"My midifile", midi_track_event> snd;
} inputs;
```

## Callback
Like other ports, it is possible to get an update callback, by implementing an `update` method ;
the simplest way is to make an empty struct which inherits from `halp::midifile_port`

```cpp
struct : halp::midifile_port<"My midifile"> {
  void update(MyObject& obj) {
    // This code is called whenever the midifile has been changed by the user
  }
} port_name;
```
