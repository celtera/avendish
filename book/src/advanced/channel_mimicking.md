# Audio channel mimicking

> Supported bindings: ossia, VST, Clap, Max/MSP

As discussed when introducing audio ports, for the sake of simplicity, a processor with one input and one output audio bus, if it does not specify a specific channel count, is assumed to have as many input as it has output channels.

For instance, consider the following case:

```cpp
struct
{
  halp::audio_input_bus<"Main Input"> audio;
} inputs;

struct
{
  halp::audio_output_bus<"Output"> audio;
} outputs;
```

Here, everything is fine: the host can send 1, 2, ... channels to the input, and Avendish will make sure that the audio output matches that.

Now imagine that we add another bus: 

```cpp
struct ins
{
  halp::audio_input_bus<"Main Input"> audio;
  halp::audio_input_bus<"Sidechain"> sidechain;
} inputs;

struct outs
{
  halp::audio_output_bus<"Output"> audio;
} outputs;
```

Even if for us, humans, it is reasonable to assume that there will be as many output channels, as there are in the main input, it is not something that a computer can assume that easily.

Thus, there is a way to indicate that a given port will use the same channel count as a specific input.

In raw terms, this is done by adding the following function in the output port definition:

```cpp
static constexpr auto mimick_channel = &ins::audio;
```

Taking the member function pointer to an input will allow Avendish to match the channel count at run-time.

An helper is provided: for instance, in the above case, it would give: 

```cpp
struct ins
{
  halp::audio_input_bus<"Main Input"> audio;
  halp::audio_input_bus<"Sidechain"> sidechain;
} inputs;

struct outs
{
  halp::mimic_audio_bus<"Output", &ins::audio> audio;
} outputs;
```
