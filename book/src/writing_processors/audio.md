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