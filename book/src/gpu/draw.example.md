# Draw example

This example draw a triangle with two controls.

- One control is an uniform exposed directly to the object.
- Another is a CPU-only control which the author of the node then maps to another uniform in a custom way.
- The texture also comes from the node's code.

```cpp
{{ #include ../../../examples/Gpu/DrawWithHelpers.hpp }}
```
