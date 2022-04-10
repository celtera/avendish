# Summary

- [Foreword](./foreword.md)

# Getting started
- [Hello World](./getting_started/hello_world.md)
- [Compiling](./getting_started/compiling.md)
- [Running](./getting_started/running.md)

# Writing CPU processors
- [Adding ports](./writing_processors/ports.md)
  - [Simplifying ports](./writing_processors/ports.refactoring.md)
  - [Helpers for ports](./writing_processors/ports.helpers.md)
  - [Port metadatas](./writing_processors/ports.metadatas.md)
    - [Range](./writing_processors/ports.metadatas.range.md)
    - [Widget](./writing_processors/ports.metadatas.widget.md)
    - [Helpers](./writing_processors/ports.metadatas.helpers.md)
- [Audio](./writing_processors/audio.md)
  - [Monophonic processors](./writing_processors/audio.polyphonic.md)
  - [Setup](./writing_processors/audio.setup.md)
  - [Playback state](./writing_processors/audio.arguments.md)
- [Messages](./writing_processors/messages.md)
- [Callbacks](./writing_processors/callbacks.md)
- [Initialization](./writing_processors/init.md)
- [MIDI](./writing_processors/midi.md)
  - [Example](./writing_processors/midi.example.md)
- [Image](./writing_processors/images.md)
  - [Example](./writing_processors/images.example.md)
- [Metadatas](./writing_processors/metadatas.md)

# Advanced features
- [Port data types](./advanced/port_types.md)
  - [Example](./advanced/port_types.example.md)
- [Custom UI](./advanced/ui.md)
  - [Declarative layouts](./advanced/ui.layout.md)
  - [Custom items](./advanced/ui.painting.md)
  - [Message bus](./advanced/ui.messages.md)
    - [Example](./advanced/ui.messages.example.md)
- [Feature injection](./advanced/injection.md)
  - [Logging](./advanced/logging.md)
  - [FFT](./advanced/fft.md)
- [Presets](./advanced/presets.md)
- [Sample-accurate processing](./advanced/sample_accurate.md)
  - [Example](./advanced/sample_accurate.example.md)
- [Channel mimicking](./advanced/channel_mimicking.md)
- [CMake configuration](./advanced/cmake.md)

# Writing GPU processors
- [Draw nodes](./gpu/draw.md)
  - [Defining a layout](./gpu/draw.layout.md)
  - [API calls](./gpu/draw.calls.md)
  - [Minimal pipeline](./gpu/draw.minimal.md)
  - [Complete example](./gpu/draw.example.md)
- [Compute nodes](./gpu/compute.md)

<!--
# Reference

- [Processing semantics](./reference/processing.md)
- [Bindings](./reference/bindings.md)
- [Controls](./reference/controls.md)

-->