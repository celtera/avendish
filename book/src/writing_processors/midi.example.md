# MIDI synth example

This example is a very simple synthesizer. Note that for the sake of simplicity for the implementer, we use two additional libraries: 

- [`libremidi`](https://github.com/jcelerier/libremidi) provides an useful `enum` of common MIDI messages types.
- [`libossia`](https://github.com/ossia/libossia) provides frequency <-> MIDI note and gain <-> MIDI velocity conversion operations.

```cpp
{{ #include ../../../examples/Tutorial/Synth.hpp }}
```
