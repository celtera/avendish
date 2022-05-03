# Writing audio processors

The processors we wrote until now only processed "control" values.

As a convention, those are values that change infrequently, relative to the audio rate: every few milliseconds, as opposed to every few dozen microseconds for individual audio samples.

## Argument-based processors
Let's see how one can write a simple audio filter in Avendish: 

```cpp
struct MyProcessor
{
  static consteval auto name() { return "Distortion"; }

  float operator()(float input) 
  {
    return std::tanh(input); 
  }
};
```

That's it. That's the processor :-)

Maybe you are used to writing processors that operate with buffers of samples. Fear not, here is another valid Avendish audio processor, which should reassure most readers:

```cpp
struct MyProcessor
{
  static consteval auto name() { return "Distortion"; }
  static consteval auto input_channels() { return 2; }
  static consteval auto output_channels() { return 2; }

  void operator()(double** inputs, double** outputs, int frames)
  {
    for (int c = 0; c < input_channels(); ++c)
    {
      for (int k = 0; k < frames; k++)
      {
        outputs[c][k] = std::tanh(inputs[c][k]);
      }
    }
  }
};
```

The middle-ground of a processor that processes a single channel is also possible (and so is the possibility to use floats or doubles for the definition of the processor):

```cpp
struct MyProcessor
{
  static consteval auto name() { return "Distortion"; }

  void operator()(float* inputs, float* outputs, int frames)
  {
    for (int k = 0; k < frames; k++)
    {
      outputs[k] = std::tanh(inputs[k]);
    }
  }
};
```

Those are all ways that enable quickly writing very simple effects (although a lot of ground is already covered).
For more advanced systems, with side-chains and such, it is preferable to use proper ports instead.

## Port-based processors

Here are three examples of valid audio ports:

* Sample-wise

```cpp
struct {
  static consteval auto name() { return "In"; }
  float sample{};
};
```

* Channel-wise

```cpp
struct {
  static consteval auto name() { return "Out"; }
  float* channel{};
};
```

* Bus-wise, with a fixed channel count. Here, bindings will ensure that there are always as many channels allocated.

```cpp
struct {
  static consteval auto name() { return "Ins"; }
  static constexpr int channels() { return 2; }
  float** samples{}; // At some point this should be renamed bus...
};
```

* Bus-wise, with a modifiable channel count. Here, bindings will put exactly as many channels as the end-user of the software requested ; this count will be contained in `channels`. 

```cpp
struct {
  static consteval auto name() { return "Outs"; }
  int channels = 0;
  double** samples{}; // At some point this should be renamed bus...
};
```

> An astute reader may wonder why one could not fix a channel count by doing `const int channels = 2;` instead of `int channels() { return 2; };`. Sadly, this would make our types non-assignable, which makes things harder. It would also use bytes for each instance of the processor. A viable middle-ground could be `static constexpr int channels = 2;` but C++ does not allow static variables in unnamed types, thus this does not leave a lot of choice.

## Process function for ports

For ports-based processor, the process function takes the number of frames as argument. Here is a complete, bare example of a gain processor. 

```cpp
struct Gain {
  static constexpr auto name() { return "Gain"; }
  struct {
    struct {
      static constexpr auto name() { return "Input"; }
      const double** samples;
      int channels;
    } audio;

    struct {
      static constexpr auto name() { return "Gain"; }
      struct range {
        const float min = 0.;
        const float max = 1.;
        const float init = 0.5;
      };

      float value;
    } gain;
  } inputs;

  struct {
    struct {
      static constexpr auto name() { return "Output"; }
      double** samples;
      int channels;
    } audio;
  } outputs;
  
  void operator()(int N) {
    auto& in = inputs.audio.samples;
    auto& out = outputs.audio.samples;

    for (int i = 0; i < p1.channels; i++) 
      for (int j = 0; j < N; j++) 
        out[i][j] = inputs.gain.value * in[i][j];
  }
};
```

## Helpers

`halp` provides helper types for these common cases: 

```cpp
halp::audio_sample<"A", double> audio;
halp::audio_channel<"B", double> audio;
halp::fixed_audio_bus<"C", double, 2> audio;
halp::dynamic_audio_bus<"D", double> audio;
```

> Important: it is not possible to mix different types of audio ports in a single processor: audio sample and audio bus operate necessarily on different time-scales that are impossible to combine in a single function. Technically, it would be possible to combine audio channels and audio buses, but for the sake of simplicity this is currently forbidden.

> Likewise, it is forbidden to mix float and double inputs for audio ports (as it simply does not make sense: no host in existence is able to provide audio in two different formats at the same time).

## Gain processor, helpers version

The exact same example as above, just shorter to write :)
```cpp
struct Gain {
  static constexpr auto name() { return "Gain"; }
  struct {
    halp::dynamic_audio_bus<"Input", double> audio;
    halp::hslider_f32<"Gain", halp::range{0., 1., 0.5}> gain;
  } inputs;

  struct {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;
  
  void operator()(int N) {
    auto& in = inputs.audio;
    auto& out = outputs.audio;
    const float gain = inputs.gain;

    for (int i = 0; i < in.channels; i++) 
      for (int j = 0; j < N; j++) 
        out[i][j] = gain * in[i][j];
  }
};
```

## Further work
We currently have the following matrix of possible forms of audio ports: 

|          | 1 channel         | N channels         |
|----------|-------------------|--------------------|
| 1 frame  | `float sample;`   | `???`              |
| N frames | `float* channel;` | `float** samples;` |

For the N channels / 1 frame case, one could imagine for instance: 

```cpp
struct {
  float bus[2]; // Fixed channels case
}
```

or 

```cpp
struct {
  float* bus; // Dynamic channels case
}
```

to indicate a per-sample, multi-channel bus, but this has not been implemented yet.
