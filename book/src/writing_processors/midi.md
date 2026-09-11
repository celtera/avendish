# MIDI I/O

> Supported bindings: ossia, vst, vst3, clap

Some media systems may have a concept of MIDI input / output. Note that currently this is only implemented for DAW-ish bindings: ossia, VST3, CLAP... Max and Pd do not support it yet (but if there is a standard for passing MIDI messages between objects there I'd love to hear about it !).

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


## Message lifetime and sliced ossia execution

MIDI ports are scratch storage for the current invocation. Keep pending starts,
active-note identity, future release deadlines, and cleanup debt in the processor,
not in a port. Do not retain pointers, references, or iterators into its messages.
Output timestamps are slice-relative and must lie in `[0, frames)`; the ossia
binding adds the slice offset once without changing the processor's timestamp.
A one-sample note starting on the last frame therefore releases at frame zero of
the next writable invocation. A zero-frame invocation cannot publish that release.

The ossia graph initializes native ports, runs all requested token slices, then
publishes the node's aggregate output. Processors that need all slices preserved
can opt in:

```cpp
halp_meta(local_midi_tick_batch, true)
halp_meta(local_midi_tick_batch_reserve, 4096) // Optional per-MIDI-port reserve.
```

This stores only already-produced packets in executor-local storage. The
`ossia::graph_node::begin_execution()` hook resets it before each graph execution,
independently of seeks, repeated positions, callback counters, and buffer sizes.
The native output is rebuilt from the batch even for empty slices. Bindings
without the opt-in retain per-slice replacement; an explicit `false` disables
batching. Direct users of `safe_node::run()` must call `begin_execution()` once
before each group of slices they will publish together. This requires a libossia
version providing that graph hook. A reserve is a capacity hint, not a bound or
an allocation-free guarantee for arbitrary host traffic.

A lifecycle callback such as `stop()` is not itself a writable publication
window. A host must run and publish a final release-only invocation before
disconnecting or destroying a processor that owns notes. In score's current
hard-stop/removal path, such an invocation is not automatically supplied;
locally queued cleanup releases alone cannot guarantee delivered note-offs.
