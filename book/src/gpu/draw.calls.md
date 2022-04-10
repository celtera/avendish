# API calls

In general, programming for the GPU involves some level of calling into a graphics API: OpenGL, Vulkan, Metal, or various abstractions on top of them such as the Qt, Unreal, etc... RHIs, libraries such as BGFX, etc.

Like every non-declarative code, this has the sad side-effect of tying you to said library ; it's pretty hard to migrate to, say, Qt's RHI to pure OpenGL or BGFX.
Here, we propose a method that aims to keep the algorithms reusable by having them state their needs without making any API calls.

The API comes with three optional methods that can be reimplemented to control behaviour for now: 

```cpp
// Allocates ressources and sends data from the CPU to the GPU.
// This method *must* be implemented if there are uniforms or samplers that 
// aren't bound to ports (and the pipeline wouldn't be very useful otherwise)
/* ??? */ update();

// Releases allocated ressources
// This method must take care of releasing anything allocated in update()
/* ??? */ release();

// Submit a draw call. If not implemented a default draw call will be done 
// for the mesh in input of the node.
/* ??? */ draw();
```

## Defining a call

Here is how an `update()` method which allocates and update a texture may look:

```cpp
gpp::texture_handle tex_handle{};

gpp::co_update update()
{
    int sz = 16*16*4;
    // If the texture hasn't been allocated yet
    if(!tex_handle)
    {
      // Request an allocation
      this->tex_handle = co_yield gpp::texture_allocation{
          .binding = 0
        , .width = 16
        , .height = 16
      };
    }

    // Generate some data
    tex.resize(sz);
    for(int i = 0; i < sz; i++)
      tex[i] = rand();

    // Upload it to the GPU
    co_yield gpp::texture_upload{
        .handle = tex_handle
      , .offset = 0
      , .size = sz
      , .data = tex.data()
    };
  }
}
```

## Commands
Note that there isn't any direct API call here. Instead, we return user-defined structs: 
for instance, `texture_allocation` is simply defined like this:

```cpp
struct texture_allocation
{
  // Some keywords to allow the command to be matched to an API call
  enum { allocation, texture };

  // What this call is supposed to return
  using return_type = texture_handle;

  // Parameters of the command
  int binding;
  int width;
  int height;
};
```

The actual concrete type does not matter: the only important thing is for the following to be possible with the returned struct `C`:

```cpp
// Identify the command
C::allocation; 
C::texture;

// Create a return value
typename C::return_type ret;

// Access the parameters of the call
void f(C& command) {
  int b = command.binding;
  int w = command.weight;
  int h = command.height;
  // etc.
}
```

This allows complete independence from the graphics API, as a node only specifies exactly the allocation / update / draw calls it needs to do in the most generic possible way ; for instance, a node that only allocates and uploads a texture should be easily bindable to any graphics API on earth.

An in-progress set of common commands is provided.

Note also that this gives some amount of named-parameter-ness which is also a good way to reduce bugs :-)

## Coroutines

To allow this to work, `gpp::co_update` is a coroutine type.
Here too, the binding code does not depend on the concrete type of the coroutine ; only that it matches a concept.

`gpp::co_update` is defined as:

```cpp
// All the possible commands that can be used in update()
using update_action = std::variant<
  static_allocation, static_upload,
  dynamic_vertex_allocation, dynamic_vertex_upload, buffer_release,
  dynamic_index_allocation, dynamic_index_upload,
  dynamic_ubo_allocation, dynamic_ubo_upload, ubo_release,
  sampler_allocation, sampler_release,
  texture_allocation, texture_upload, texture_release,
  get_ubo_handle
>;

// What the commands are allowed to return
using update_handle = std::variant<std::monostate, buffer_handle, texture_handle, sampler_handle>;

// Definition of the update() coroutine type
using co_update = gpp::generator<update_action, update_handle>;
```

Where `gpp::generator` is a type similar to `std::generator` which is not available yet in C++20 but will be in C++23.

This has interesting benefits: for instance, it allows to restrict what kind of call can be done in which function.
For instance, the Qt RHI forbids uploading data during a draw operation: the coroutine type for `draw` does not contain 
the update commands, which allows to enforce this at compile-time. Yay C++ :-)

A node which only ever uploads textures could optimize a little bit by defining instead: 

```cpp
using update_action = std::variant<texture_allocation, texture_upload, texture_release>;
using update_handle = std::variant<std::monostate, texture_handle>;

using my_co_update = gpp::generator<update_action, update_handle>;
```

> Of course, we would love this to be performed automatically as part of compiler optimizations... it seems that the science is not there yet though !

# How does it work ??

It's very simple: the code which invokes `update()` more-or-less looks like this:

```cpp
void invoke_update()
{
  if constexpr(requires { node.update(); })
  {
    for (auto& promise : node.update())
    {
      promise.result = visit(update_handler{}, promise.command);
    }
  }
}
```

where `update_handler` looks like:

```cpp
struct update_handler
{
  template<typename C>
  auto operator()(C command) {
    if constexpr(requires { C::allocation; })
    {
      if constexpr(requires { C::texture; })
        my_gpu_api_allocate_texture(command.width, command.height);
      else if constexpr(requires { C::ubo; })
        my_gpu_api_allocate_buffer(command.size);
      else ...
    }
    else ...
  }
};
```

One can check that given the amount of abstraction involved, the compiler can still generate [reasonable amounts of code](https://gcc.godbolt.org/z/rcqT91d7f) for this (when provided with a decent std::variant implementation :p).


> Note that the implementation does not depend on the variant type being `std::variant` -- I also tested with `boost.variant2` and `mpark::variant` which follow the same concepts.
