# Avendish examples

This folder contains multiple examples:

- [Tutorial](Tutorial) is a set of step-by-step tutorials to learn how to write simple audio, data and texture processors with the library, detailed below.
- [Demos](Demos) contains example programs.
- [Ports](Ports) contains more complete examples ported from other libraries.
- [Helpers](Helpers) contains various examples mainly used for testing the helpers library.
- [Raw](Raw) contains various examples mainly used for testing raw plug-ins.

## Simple examples

These examples showcase the basic usage of the API for the most common cases of processors found in
multimedia systems.

- [1 - Hello World](Tutorial/EmptyExample.hpp)
- [2 - Trivial value generator](Tutorial/TrivialGeneratorExample.hpp)
- [3 - Trivial value filter](Tutorial/TrivialFilterExample.hpp)
- [4 - Simple audio effect](Tutorial/AudioEffectExample.hpp)
- [5 - Audio effect with side-chains](Tutorial/AudioSidechainExample.hpp)
- [6 - MIDI Synthesizer](Tutorial/Synth.hpp)
- [7 - Texture Generator](Tutorial/TextureGeneratorExample.hpp)
- [8 - Texture Filter](Tutorial/TextureFilterExample.hpp)

## Advanced features

These examples showcase some more advanced features:

* Sample-accurate control values:
- [9 - Sample-accurate generator](Tutorial/SampleAccurateGeneratorExample.hpp)
- [10 - Sample-accurate filter](Tutorial/SampleAccurateFilterExample.hpp)
- [11 - Porting an effect from Max/MSP: CCC](Ports/LitterPower/CCC.hpp)

* Dynamic multichannel handling:
- [12 - Multichannel distortion](Tutorial/Distortion.hpp)

* All the GUI controls that can currently be used
- [13 - Control Gallery](Tutorial/ControlGallery.hpp)

## Going deeper

* How to write a plug-in without having any dependency at all:
- [14 - Zero-dependency audio effect](Tutorial/ZeroDependencyAudioEffect.hpp)

The content of the "Raw" folder contains a set of examples without using helpers to show how 
every feature of the library works underneath - they are all valid Avendish plug-ins !