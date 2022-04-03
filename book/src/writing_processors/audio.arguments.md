# Audio arguments

In addition of the global set-up step, one may require per-process-step arguments.
Most common needs are for instance the current tempo, etc.

The infrastructure put in place for this is very similar to the one previously mentioned for 
the setup step.

The way it is done is simply by passing it as the last argument of the processing `operator()` function.

If there is such a type, it will contain at least the frames.

> Note: due to a lazy developer, currently this type has to be called `tick`.

Example:

```cpp
struct MyProcessor {
  ...

  struct tick {
    int frames;
    double tempo;
  };

  void operator()(tick tick) { ... }
  float operator()(float in, tick tick) { ... }
  void operator()(float* in, float* out, tick tick) { ... }
  void operator()(float** in, float** out, tick tick) { ... }

// And also the variants that take input and output types as arguments

  void operator()(const inputs& in, outputs& out, tick tick) { ... }
  float operator()(float in, const inputs& in, outputs& out, tick tick) { ... }
  void operator()(float* in, float* out, const inputs& in, outputs& out, tick tick) { ... }
  void operator()(float** in, float** out, const inputs& in, outputs& out, tick tick) { ... }
};
```

The currently supported members are: 

- `frames`: the buffer size

The plan is to introduce: 

- `tempo` and all things relative to musicality, e.g. current bar, etc. 
  - But first we have to define it in a proper way, which is compatible with VST, CLAP, etc.
- `time_since_start`
- and other similar timing-related things which will all be able to be opt-in.