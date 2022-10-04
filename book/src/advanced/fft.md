# FFT feature

> Supported bindings: ossia

FFT operates on complex numbers ; you can expect a `real()` and `complex()` members.
Here is a simple example, which looks for the band with the most amplitude.

```cpp
{{ #include ../../../examples/Helpers/PeakBand.hpp }}
```

> Avendish currently provides a very simple and unoptimized FFT for the sake of testing. Contributions of bindings to more efficient FFT libraries are very welcome :-)
