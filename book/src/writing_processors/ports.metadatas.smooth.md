# Smoothing values

> Supported bindings: ossia

It is common to require controls to be smoothed over time, in order to prevent clicks and pops in the sound.

Avendish allows this, by defining a simple `smooth_ratio` which specifies over how many milliseconds the control changes must be smoothed.

```cpp
struct {
  static consteval auto name() { return "foobar"; }
  static consteval auto smooth_ratio() { return 100.; }

  struct range {
    float min = 20.;
    float max = 20000.;
    float init = 100.;
  };

  float value{};
} foobar;
```
