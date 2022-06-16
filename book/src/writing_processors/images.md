# Image ports

Some media systems have the ability to process images. Avendish is not restricted here :-)

Note that this part of the system is still pretty much in flux, in particular with regards of how allocations are supposed to be handled.
Any feedback on this is welcome.

First, here is how we define a viable texture type:

```cpp
struct my_texture
{
  enum format { RGBA }; // The only recognized one so far
  unsigned char* bytes;
  int width;
  int height;
  bool changed;
};
```

Then, a texture port:

```cpp
struct {
  rgba_texture texture;
} input;
```

Note that currently, it is *the responsibility of the plug-in author* to allocate the texture and set the `changed` bool for output ports. Input textures come from outside.

Due to the large cost of uploading a texture, `changed` is used to indicate both to the plug-in author that input textures have been touched,and for the plug-in author to indicate to the external environment that the output has changed and must be re-uploaded to the GPU.


## GPU processing

Check the [Writing GPU Processors](../gpu/draw.md) chapter of this book!

## Helpers

A few types are provided:

* `halp::rgba_texture`
* `halp::texture_input<"Name">` provides methods to get an RGBA pixel: 

```cpp
auto [r,g,b,a] = tex.get(10, 20);
```

* `halp::texture_output<"Name">` provides methods to set a RGBA pixel: 

```cpp
tex.set(10, 20, {.r = 10, .g = 100, .b = 34, .a = 255});
tex.set(10, 20, 10, 100, 34, 255);
```

as well as useful method to initialize and mark the texture ready for upload: 

```cpp
// Call this in the constructor or before processing starts
tex.create(100, 100);

// Call this after making changes to the texture
tex.upload();
```

