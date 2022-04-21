# Soundfile ports

Soundfile ports are currently only supported with the ossia binding.

They allow to define an input which will be the content of a sound file, along with its metadata: number of channels, number of frames.

## Port definition

A soundfile input port looks like this:

```cpp
struct
{
  static consteval auto name() { return "My soundfile"}
  struct {
    const float** data{}; // The audio channels
    int64_t frames{}; // How many samples are there in a channel
    int32_t channels{}; // How many channels are there
    std::string_view filename; // Currently loaded soundfile
  } soundfile;
} snd;
```

A helper type is provided:

```cpp
struct {
  halp::soundfile<"My soundfile"> snd;
} inputs;

void operator()(int frames) {
  // Check if a soundfile is loaded:
  if(!inputs.snd)
    return;

  // Access things through helpers
  const int channels = inputs.snd.channels();
  const int frames = inputs.snd.frames();
  std::span<const float> chan = inputs.snd.channel(0);

  // Or manually...
  for(int i = 0; i < channels; i++)
    for(int j = 0; j < frames; j++)
      inputs.snd[i][j];
}
```
