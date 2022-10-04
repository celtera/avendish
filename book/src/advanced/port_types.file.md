# File ports

> Supported bindings: ossia

File ports are currently only supported with the ossia binding.

They allow to specify that we want the content of a file.
The host will take care of loading, mmaping, converting, etc... the file in the relevant format - as far as possible outside of DSP / processing threads.

Every file port should at least conform to the very basic following concept: 

```cpp
template <typename T>
concept file = requires(T t) {
  t.filename;
};

template <typename T>
concept file_port = requires(T t) {
  { t.file } -> file;
};
```

Here is for instance a valid basic file port ; hosts will open a relevant file chooser:

```cpp
struct my_file_port {
  struct {
    std::string filename;
  } file;
};
```

## Features

### File watch

By adding a `file_watch` [flag](../development/flags.md) to the class, the host environment will reload the file's data whenever a change is detected on disk.
Note that this has an obvious performance cost as the host must check regularly for changes: do not overdo it!

### File filters

File ports should prompt the host to open a file chooser.
One is generally only interested in a specific file type. 
This can be specified in the following ways: 

#### General file filters

Here, the filter is a string of either of the following formats:

```
"*.jpg *.jpeg"
"JPEG Images (*.jpg *.jpeg)"
```

```cpp
struct my_file_port : ... {
  // Currently implemented:
  // Any audio file that can be loaded by the host
  static consteval auto filters() { return "CSV files (*.csv)"; }
};
```

#### Domain-specific file filters

This is for the case where you want a general kind of media and don't care specifically about the extension: 
for instance, for audio files you could have .wav, .aiff, .mp3, etc etc ; it is pointless to try to specify a whole list.
Instead, the host knows which audio file formats it is able to load and should just be able to show its own audio-specific file chooser.

```cpp
struct my_file_port : ... {
  // Currently implemented:
  // Any audio file that can be loaded by the host
  static consteval auto filters() { enum { audio }; return audio; }

  // Any midi file that can be loaded by the host
  static consteval auto filters() { enum { midi }; return midi; }

  // In the future:
  static consteval auto filters() { enum { image }; return image; }
  static consteval auto filters() { enum { video }; return video; }
};
```
