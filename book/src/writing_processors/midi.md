# MIDI I/O

Some media systems may have a concept of MIDI input / output.

There are a few ways to specify MIDI ports.

Here is how one specifies unsafe MIDI ports:

```cpp
struct
{
  static consteval auto name() { return "MIDI"; }
  struct
  {
    uint8_t bytes[3]{};
    int timestamp{}; // relative to the beginning of the tick
  }* midi_messages{};
  std::size_t size{};
} midi_port;
```

Or, more clearly:

```cpp
// the name does not matter
struct midi_message {
  uint8_t bytes[3]{};
  int timestamp{}; // relative to the beginning of the tick
};

struct
{
  static consteval auto name() { return "MIDI"; }
  midi_message* midi_messages{};
  std::size_t size{};
} midi_port;
```

Here, Avendish bindings will allocate a large enough buffer to store MIDI messages ; this is mainly to enable writing dynamic-allocation-free backends where such a buffer may be allocated statically. 

It is also possible to do this if you don't expect to run your code on Arduinos:

```cpp
struct
{
  // Using a non-fixed size type here will enable MIDI messages > 3 bytes, if for instance your 
  // processor expects to handle SYSEX messages.
  struct msg {
    std::vector<uint8_t> bytes;
    int64_t timestamp{};
  };

  std::vector<msg> midi_messages;
} midi_port;
```

## Helpers
The library provides helper types which are a good compromise between these two solutions, as they are based on `boost::container::small_vector`: for small numbers of MIDI messages, there will be no memory allocation, but pathological cases (an host sending a thousand MIDI messages in a single tick) can still be handled without loosing messages. 

The type is very simple:

```cpp
halp::midi_bus<"In"> midi;
```


