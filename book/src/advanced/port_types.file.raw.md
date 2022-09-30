# Raw file ports

File ports are currently only supported with the ossia binding.

They allow to define an input which will be the content of a file on the hard drive.
The file is memory-mapped.

## Port definition

A file input port can be defined like this:

```cpp
struct
{
  static consteval auto name() { return "My file"}
  struct {
    // The file data. 
    // Only requirement for the type is that it can be assigned {data, size}.
    std::string_view bytes; 

    std::string_view filename; // Currently loaded file

    // Optional: if defined, the file will be memory-mapped instead of being copied in RAM, 
    // which can be relevant for very large assets.
    // Be aware of the page fault implications before accessing the data in the audio code !
    enum { mmap }; 

    // Optional: if defined, the file will be read in text mode, e.g. \r\n (Windows line endings) 
    // get translated to \n on Linux & MacOS. Incompatible with mmap, for obvious reasons.
    // Default is reading in binary mode.
    enum { text }; 
  } file;
} snd;
```

A helper type is provided:

```cpp
struct {
  halp::file_port<"My file"> snd;
} inputs;
```

## Callback
Like other ports, it is possible to get an update callback, by implementing an `update` method ;
the simplest way is to make an empty struct which inherits from `halp::file_port`

```cpp
// text_file_view (the default) has the text flag, and mmap_file_view has the mmap flag.
struct : halp::file_port<"My file", mmap_file_view> {
  void update(MyObject& obj) {
    // This code is called whenever the file has been changed by the user
  }
} port_name;
```
