# Smoothing values

> Supported bindings: ossia

It is common to require controls to be smoothed over time, in order to prevent clicks and pops in the sound.

Avendish allows this, by defining a simple `smoother` struct which specifies over how many milliseconds the control changes must be smoothed.

```cpp
struct {
  static consteval auto name() { return "foobar"; }

  struct smoother {
    float milliseconds = 10.;
  };

  struct range {
    float min = 20.;
    float max = 20000.;
    float init = 100.;
  };

  float value{};
} foobar;
```

It is also possible to get precise control over the smoothing ratio, depending on the control update rate (sample rate or buffer rate).


# Helpers

Helper types are provided: 


```cpp
// #include: <halp/smoothers.hpp>

{{ #include ../../../include/halp/smoothers.hpp }}
```

# Usage example

These examples smooth the gain control parameter, either over a buffer in the first case, or for each sample in the second case.

```cpp
{{ #include ../../../examples/Helpers/SmoothGain.hpp }}
```
