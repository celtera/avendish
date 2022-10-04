# Feature injection

> Supported bindings: all

Many processors may require some kind of common, cross-cutting algorithm or system for their proper operation.

Processors can optionally declare a template argument, which will contain the implementations of these "cross-cutting concerns" supported by the backend.

For now, there are two: 

- A logging system
- An 1D FFT 

```cpp
template<typename Conf>
// Get a compile error if the bindings cannot provide the thing.
requires (halp::has_fft_1d<Conf, double> && halp::has_logger<Conf>)
struct MyProcessor {
  using logger = typename Conf::logger_type;
  using fft_type = typename Conf::template fft_type<double>;
};
```

This means that for instance, a processor can log to the Pd or Max console through `post(...)`, to stdout on Python, etc. and that they are relieved of the need to go look for an FFT algorithm.

Hosts like *ossia score* will be able to plug-in their own FFT implementation optimized for the platform on which the plug-in is running (and binaries will stop having 45 duplicate FFT implementations...).

