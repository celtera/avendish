# GPU-based draw nodes

The techniques shown so far can be extended to the general ontology of "modern GPU pipeline".

Here are some useful readings to get an idea of the problem space: 

- [https://alain.xyz/blog/comparison-of-modern-graphics-apis](https://alain.xyz/blog/comparison-of-modern-graphics-apis)
- [https://www.qt.io/blog/qt-quick-on-vulkan-metal-direct3d](https://www.qt.io/blog/qt-quick-on-vulkan-metal-direct3d)
- [https://docs.unrealengine.com/4.27/en-US/API/Runtime/RHI/](https://docs.unrealengine.com/4.27/en-US/API/Runtime/RHI/)
- [https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/](https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/)
- [https://www.o3de.org/docs/atom-guide/dev-guide/rhi/](https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/)

## Defining a primitive pipeline ontology

A GPU pipeline carries much more information than the average audio object, as GPUs are pretty configurable and specialized systems, with various possible qualifications on their inputs and outputs that are unfamiliar to the programmer accustomed to working with a CPU.

The core of a GPU pipeline is the program that will run on it: a shader. Shaders are in themselves very similar to data-flow objects: they define inputs (often the mesh data, textures, and parameters to control the rendering) and outputs (often the color). 

The pipeline may have multiple stages: most commonly and in an extremely broad and inaccurate way, the vertex stage positions triangles in the GPU API's coordinate system, and the fragment stage renders these triangles.

The pipeline has a layout: it is the definition of its inputs and outputs. For instance, such a layout may be:

- A vec2 attribute input at location 0 representing each vertex's position.
- A vec4 attribute input at location 1 representing each vertex's color.

- A texture at binding 0.
- A buffer at binding 1 containing parameters for drawing the texture. For instance:

```glsl
uniform my_parameters {
  vec4 global_color;
  vec2 sun_position;
  float brightness;
};
```

The way it conceptually works is that, for each vertex of each triangle, the vertex shader is called with that vertex, plus the shared global parameters bound. Here is an example

```glsl
// Input attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

// Outputs of the vertex shader
layout(location = 0) out vec3 v_color;

// Where we tell the GPU at which position the current 
// fragment is.
out gl_PerVertex { vec4 gl_Position; };

// Shared parameters
layout(std140, binding = 0) uniform my_parameters {
  vec4 global_color;
  vec2 sun_position;
  float brightness;
};

// The actual vertex program
void main() 
{
  // Do some math that transforms pos's coordinates to show it where we want.
  vec4 pos = vec4(position.xt, 0.0, 1.);
  /* ... many matrix multiplications go here ... */
  gl_Position = pos;

  // Just copy the associated color directly to the next stage
  v_color = color;
}
```

Once this is done, the outputs of this stage are passed to the fragment shader, which 
will end up computing the color that is to be shown on screen: 

```glsl
// Input of the fragment shader ; must be the same name, types and locations
// than the output of the vertex shader
layout(location = 0) in vec3 v_color;

// Output of the fragment shader: a color
layout(location = 0) out vec4 fragColor;

// Shared parameters ; must be the same name, types and locations
// than in the vertex shader
layout(std140, binding = 0) uniform my_parameters {
  vec4 global_color;
  vec2 sun_position;
  float brightness;
};

layout(binding = 1) uniform sampler2D tex;

// The actual fragment program
void main() {
  vec4 color = v_color
             + global_color
             + texture(tex, gl_FragCoord);

  fragColor = brightness * color;
}
```

The main problem is ensuring that the pipeline's layout as used from the C++ code matches the pipeline's layout as declared in the shader ; as every time there is an information duplication in a system, there is the possibility for errors there.

Here, we have duplication: 

- Between the outputs of the vertex and the input of the fragment - some drivers and APIs are lenient regarding errors here, others are not.
- In the C++ side, which must have some way to tell the GPU:
* The first attribute  will be a vec2
* The second attribute will be a vec4
* The texture is bound at position 1
* etc etc.

The idea is that we can use the same principles than saw before to define our layout through a C++ struct, and get our library to automatically generate the relevant inputs and outputs.