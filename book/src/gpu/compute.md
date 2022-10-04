# Compute nodes

> Supported bindings: ossia

Compute is simpler than draw, as the pipeline only has one shader (the compute shader).
Instead of `draw`, the method in which to run compute dispatch calls is called `dispatch`.

Here is an example:

```cpp
{{ #include ../../../examples/Gpu/Compute.hpp }}
```
