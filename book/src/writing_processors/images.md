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

... is not currently supported properly, but is under active investigation.

The two possibilities going forward are: 

- Wrap / conceptify an API not dissimilar to abstraction APIs such as [BGFX](https://github.com/bkaradzic/bgfx) or [Qt RHI](https://www.qt.io/blog/graphics-in-qt-6.0-qrhi-qt-quick-qt-quick-3d) which abstract over modern graphics APIs such as Vulkan, Metal and D3D (and retain compatibility with OpenGL).
- Make bindings that support compilation to GPU programs, for instance with CUDA or [Circle](https://github.com/seanbaxter/shaders/blob/master/README.md). This is what I think is the best way going forward :-)