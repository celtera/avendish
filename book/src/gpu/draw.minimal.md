# Minimal shaders

Sometimes only very simple fragment shaders are needed.

In this case, one can omit all the functions and most of the pipeline layout, 
and only define for instance an UBO, samplers and a fragment shader.

```cpp
{{ #include ../../../examples/Gpu/SolidColor.hpp }}
```
