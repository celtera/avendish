# Presets

> Supported bindings: VST

An experimental presets feature has been prototyped for the Vintage back-end.

Here is how one may define presets:

```cpp
// Our inputs
struct
{
  halp::hslider_f32<"Preamp"> preamp;
  halp::hslider_f32<"Volume"> volume;
} inputs;

// We define the type of our programs, like in the other cases
// it ends up being introspected automatically.
struct program {
  std::string_view name;
  decltype(Presets::inputs) parameters;
};

// Definition of the presets:
// Note: it's an array instead of a function because
// it's apparently hard to deduce N in array<..., N>, unlike in C arrays.
static constexpr const program programs[]{
    {.name{"Low gain"}, .parameters{.preamp = {0.3}, .volume = {0.6}}},
    {.name{"Hi gain"},  .parameters{.preamp = {1.0}, .volume = {1.0}}},
};
```