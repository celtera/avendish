# Defining a mapping function

It is common in audio to have a range which is best manipulated by the user in logarithmic space,
for instance for adjusting frequencies, as the human hear is more sensitive to a 100Hz variation around 100Hz, thank around 15kHz.

We can define custom mapping functions as part of the control, which will define
how the UI controls shall behave:

```cpp
struct {
  static consteval auto name() { return "foobar"; }

  struct mapper {
    static float map(float x) { return std::pow(x, 10.); }
    static float unmap(float y) { return std::pow(y, 1./10.); }
  };

  struct range {
    float min = 20.;
    float max = 20000.;
    float init = 100.;
  };

  float value{};
} foobar;
```

The mapping function must be a bijection of `[0; 1]` unto itself.

That is, the `map` and `unmap` function you choose must satisfy:
- `for all x in [0; 1] map(x) is in [0; 1]`
- `for all x in [0; 1] unmap(x) is in [0; 1]`
- `for all x in [0; 1] unmap(map(x)) == x`
- `for all x in [0; 1] map(unmap(x)) == x`

For instance, `sin(x)` or `ln(x)` do not work, but the `x^N` / `x^(1/N)` couple works.

Note that for now, mappings are only supported for floating-point sliders and knobs, in ossia.

## With helpers
A few pre-made mappings are provided: the above could be rewritten as:

```cpp
struct : halp::hslider_f32<"foobar", halp::range{20., 20000., 100.}> {
  using mapper = halp::pow_mapper<10>;
} foobar;
```

The complete list is provided in `#include <halp/mappers.hpp>`:

```cpp
{{ #include ../../../include/halp/mappers.hpp }}
```
