# Audio setup

> Supported bindings: ossia, vst, vst3, clap, Max, Pd

It is fairly common for audio systems to need to have some buffers allocated or perform pre-computations depending on the sample rate and buffer size of the system.

This can be done by adding the following method in the processor:

```cpp
void prepare(/* some_type */ info) {
    ...
}
```

`some_type` can be a custom type with the following allowed fields: 

* `rate`: will be filled with the sample rate.
* `frames`: will be filled with the maximum frame (buffer) size.
* `input_channels` / `output_channels`: for processors with unspecified numbers of channels, it will be notified here.
* Alternatively, just specifying `channels` works too if inputs and outputs are expected to be the same.
* `instance`: allows to give processor instances an unique identifier which, if the host supports it, will be serialized / deserialized across restarts of the host and thus stay constant.

Those variables must be assignable, and are all optional (remember the foreword: Avendish is **UNCOMPROMISING**).

Here are some valid examples:

- No member at all: this can be used to just notify the processor than processing is about to start.
```cpp
struct setup_a { };
void prepare(setup_a info) {
    ...
}
```

- Most common use case
```cpp
struct setup_b {
  float rate{};
  int frames{};
};
void prepare(setup_b info) {
    ...
}
```

- For variable channels in simple audio filters:
```cpp
struct setup_c {
  float rate{};
  int frames{};
  int channels{};
};
void prepare(setup_c info) {
    ...
}
```

## Helper library

`halp` provides the `halp::setup` which covers the most usual use cases: 

```cpp
void prepare(halp::setup info) {
  info.rate; // etc...
}
```

# How does this work ?

If you are interested in the implementation, it is actually fairly simple.

- First we extract the function arguments of `prepare` if the function exists (see `avnd/common/function_reflection.hpp` for the method), to get the type `T` of the first argument.
- Then we do the following if it exists:

```cpp
using type = /* type of T in prepare(T t) */;
if constexpr(requires (T t) { t.frames = 123; })
  t.frames = ... the buffer size reported by the DAW ...;
if constexpr(requires (T t) { t.rate = 44100; })
  t.rate = ... the sample-rate reported by the DAW ...;
```

This way, only the cost of the variables that are actually used by the algorithm is ever incurred, which is of course not super important but a good reference implementation for this way of doing for other parts of the system where it matters more. 
