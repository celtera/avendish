# Monophonic processors

> Supported bindings: ossia, vst, vst3, clap, Max

There are three special cases: 

1. Processors with one sample input and one sample output.
2. Processors with one channel input and one channel output.
3. Processors with one dynamic bus input, one dynamic bus output, and no fixed channels being specified.

In these three cases, the processor is recognized as polyphony-friendly. That means that in cases 1 and 2, the processor will be instantiated potentially multiple times automatically, if used in e.g. a stereo DAW.

In case 3, the channels of inputs and outputs will be set to the same count, which comes from the host.

## Polyphonic processors should use types for their I/O

Let's consider the following processor:

```cpp
struct MyProcessor
{
  static consteval auto name() { return "Distortion"; }
  struct {
    struct { float value; } gain;
  } inputs;

  double operator()(double input) 
  {
    accumulator = std::fmod(accumulator+1.f, 10.f);
    return std::tanh(inputs.gain.value * input + accumulator); 
  }
  
private:
  double accumulator{};
};
```

We have three different values involved: 

- `input` is the audio sample that is to be processed.
- `inputs.gain.value` is an external control which increases or decreases the distortion.
- `accumulator` is an internal variable used by the processing algorithm.

Now consider this in the context of polyphony: the only thing that we can do is instantiate `MyProcessor` three times.

- We cannot call `operator()` of a single instance on multiple channels, as the internal state must stay independent of the channels.
- But now the inputs are duplicated for all instances. If we want to implement a filter bank with thousands of duplicated processors in parallel, this would be a huge waste of memory if they all depend on the same `gain` value.

Thus, it is recommended in this case to use the following form: 


```cpp
struct MyProcessor
{
  static consteval auto name() { return "Distortion"; }
  struct inputs {
      struct { float value; } gain;
  };
  struct outputs { };

  double operator()(double input, const inputs& ins, outputs& outs) 
  {
    accumulator = std::fmod(accumulator+1.f, 10.f);
    return std::tanh(ins.gain.value * input + accumulator); 
  }

private:
  double accumulator{};
};
```

Here, Avendish will instantiate a single `inputs` array, which will be shared across all polyphony voices, which will likely use less memory and be more performant in case of large amount of parameters & voices.

Here is what I would term the "canonic" of this version, with additionally our helpers to reduce typing, and the audio samples passed through ports instead of through arguments:

```cpp
struct MyProcessor
{
  static consteval auto name() { return "Distortion"; }
  struct inputs {
    halp::audio_sample<"In", double> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0, .max = 100, .init = 1}> gain;
  };
  struct outputs { 
    halp::audio_sample<"Out", double> audio;
  };

  void operator()(const inputs& ins, outputs& outs) 
  {
    accumulator = std::fmod(accumulator + 0.01f, 10.f);
    outs.audio = std::tanh(ins.gain * ins.audio + accumulator); 
  }

private:
  double accumulator{};
};
```

Passing inputs and outputs as types is also possible for all the other forms described previously - everything is possible, write your plug-ins as it suits you best :) and who knows, maybe with metaclasses one would also be able to generate the more efficient form directly.