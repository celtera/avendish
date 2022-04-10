# Defining the layout

Defining the layout of a draw pipeline is fairly similar to defining the inputs and outputs of our nodes:

```cpp
struct layout
{
  // Indicate that this is a layout for a graphics pipeline
  enum { graphics };

  struct vertex_input 
  {
    struct my_first_attribute { 
      // ...
    } my_first_attribute;

    struct my_second_attribute { 
      // ...
    } my_second_attribute;

    // etc...
  };

  struct vertex_output 
  {
    // ...
  };

  struct fragment_input 
  {
    // ...
  };
  struct fragment_output 
  {
    // ...
  };
  
  struct bindings 
  {
    struct my_ubo 
    {
      // ..
    } my_ubo;

    struct my_sampler 
    {
      // ..
    } my_sampler;
  };
};
```

## Defining attributes

An attribute is defined by the matching C++ data type.

For instance:

```glsl
layout(location = 1) in vec3 v_pos;
```

is defined through the following C++ code:
```cpp
struct {
  // Name of the variable in the shader
  static constexpr auto name() { return "v_pos"; }

  // Location
  static constexpr int location() { return 1; }
 
  // Optional standardized semantic usage for compatibility with usual engines
  enum { position };

  // Corresponding data type
  float data[3];
} my_first_attribute;
```

An helper macro is provided to reduce typing:

```cpp
// location, name, type, meaning
gpp_attribute(1, v_pos, float[3], position) my_first_attribute;
```

## Defining samplers

Samplers are locations to which textures are bound during the execution of a pipeline.
They are defined in the `bindings` section of the `layout` struct.

For instance:

```glsl
layout(binding = 2) uniform sampler2D my_tex;
```

is defined through:

```cpp
struct bindings {
  struct { 
    // Name of the variable in the shader
    static constexpr auto name() { return "my_tex"; }
  
    // Location
    static constexpr int binding() { return 2; }
   
    // Type flag
    enum { sampler2D };  
  } my_sampler;
};
```

or the helper version:

```cpp
struct bindings {
  gpp::sampler<"my_tex", 2>  my_sampler;
};
```

## Defining uniform buffers

```glsl
layout(std140, binding = 2) uniform my_params {
  vec2 coords;
  float foo;
};
```

is defined as follows:

```cpp
struct bindings {
  struct custom_ubo {
    static constexpr auto name() { return "my_params"; }
    static constexpr int binding() { return 2; }
    enum { std140, ubo };
  
    struct {
      static constexpr auto name() { return "coords"; }
      float value[2];
    } coords;
  
    struct
    {
      static constexpr auto name() { return "foo"; }
      float value;
    } foo;
  } ubo;
};
```

And can be refactored a bit to:

```cpp
struct bindings {
  struct {
    halp_meta(name, "my_params");
    halp_meta(binding, 2);
    halp_flags(std140, ubo);
  
    gpp::uniform<"coords", float[2]> coords;
    gpp::uniform<"foo", float> foo;
  } ubo;
};
```

Note that this is only used as a way to enable us to synthesize the UBO layout in the shader.
In particular, depending on the types used, the GLSL variable packing rules are sometimes different than the C++ ones, 
thus we cannot just send this struct as-is to GPU memory. Maybe when metaclasses happen one will be able to write something
akin to:

```cpp
std140_struct { ... } ubo;
```

> Note that not everything GLSL feature is supported yet, but is on the roadmap: arrays, ssbos, sampler2DShadow, etc...
> Contributions welcome !
