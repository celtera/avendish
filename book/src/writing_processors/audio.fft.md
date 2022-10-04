# Audio FFT

> Supported bindings: ossia

It is pretty common for audio analysis tasks to need access to the audio spectrum.

However, this causes a dramatic situation at the ecosystem level: every plug-in ships with its own FFT 
implementation, which at best duplicates code for no good reason, and at worse may cause complex 
issues for FFT libraries which rely on global state shared across the process, such as FFTW.

With the declarative approach of Avendish, however, we can make it so that the plug-in does not 
directly depend on an FFT implementation: it just requests that a port gets its spectrum computed, and it happens 
automatically outside of the plug-in's code. This allows hosts such as [ossia score](https://ossia.io) to provide 
their own FFT implementation which will be shared across all Avendish plug-ins, which is strictly better for performance
and memory usage.

## Making an FFT port
This can be done by extending an audio input port (channel or bus) with a `spectrum` member.

For instance, given:

```cpp
struct {
  float** samples{};
  int channels{};
} my_audio_input;
```

One can add the following `spectrum` struct:

```cpp
struct {
  float** samples{};

  struct {
    float** amplitude{};
    float** phase{};
  } spectrum;

  int channels{};
} my_audio_input;
```

to get a deinterleaved view of the amplitude & phase: 

```cpp
spectrum.amplitude[0][4]; // The amplitude of the bin at index 4 for channel 0
spectrum.phase[1][2]; // The phase of the bin at index 2 for channel 1
```

It is also possible to use complex numbers instead:

```cpp
struct {
  double** samples{};

  struct {
    std::complex<double>** bins;
  } spectrum;

  int channels{};
} my_audio_input;
```

```cpp
spectrum.bins[0][4]; // The complex value of the bin at index 4 for channel 0
```

Using complex numbers allows to use the C++ standard math library functions for complex numbers: `std::norm`, `std::proj`...

Note that the length of the spectrum arrays is always `N / 2 + 1`, N being the current frame size. Note also that the FFT is normalized - the input is divided by the amount of samples.

> WIP: an important upcoming feature is the ability to make configurable buffered processors, so that 
> one can choose for instance to observe the spectrum over 1024 frames. Right now this has to be handled internally by the plug-in.

### Windowing

A window function can be applied by defining an 

```cpp
enum window { <name of the window> };
```

Supported names currently are `hanning`, `hamming`. If there is none, there will be no windowing (technically, a rectangular window). The helper types described below use an Hanning window.

> WIP: process with the correct overlap for the window size, e.g. 0.5 for Hanning, 0.67 for Hamming etc. once buffering is implemented.

## Helper types

These three types are provided. They give separated amplitude / phase arrays.
```cpp
halp::dynamic_audio_spectrum_bus<"A", double> a_bus_port;
halp::fixed_audio_spectrum_bus<"B", double, 2> a_stereo_port;
halp::audio_spectrum_channel<"C", double> a_channel_port;
```

## Accessing a FFT object globally

See [the section about feature injection](../advanced/fft.md) to see how a plug-in can be injected with an FFT object which allows to control precisely how the FFT is done.

## Example

```cpp
{{ #include ../../../examples/Helpers/PeakBandFFTPort.hpp }}
```