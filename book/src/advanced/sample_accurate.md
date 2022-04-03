# Sample-accurate processing

So far, we saw that control ports / parameters would have a single `value` member, which as one can expects, 
stays constant for at least the entire duration of a tick.

However, some hosts (such as *ossia score*) are able to give precise timestamps to control values.

If an algorithm supports this level of precision, it can be expressed by extending value ports in the following way:

```cpp
struct { 
  static consteval auto name() { return "Control"; } 

  /* a value_map type */ values;
  float value; 
} control;
```

- `value` will always carry the "running" value at the beginning of the tick, like before.
- `values` is a type which should be API-wise more-or-less compatible with `std::map<int, type_of_value>`. 

For every message received in the tick, `values` will be set (which means that they can also be empty if no message at all was received on that port).

There are actually three options for implementing `values`.

- Option A: `std::map<int, float> values`: the simplest case. Can be slow. A helper which uses `boost::small_flat_map` is provided: it provides neat performance and won't allocate unless the port is spammed. 

- Option B: `std::optional<float>* values;`: here, a `std::optional<float>` array of the same length than audio channels will be allocated. Indexing is the same than for audio samples.
  
- Option C can only be used for inputs:
  
```cpp
struct timestamped_value {
  T value;
  int frame;
 };
  std::span<timestamped_value> values;
```

This is the most efficient storage if you expect to receive few values (and also the most "my device has extremely little RAM"-friendly one), however the ability to just do `values[frame_index]` is lost as the index now only goes up to the allocated messages (which can be zero if no message was received for this tick).

# Helpers

A single helper is provided for now: `halp::accurate<T>`.

It can be used like this to wrap an existing control type to add it a sample-accurate storage buffer:

```
halp::accurate<halp::val_port<"Out", float>> my_port;
halp::accurate<halp::knob_i32<"Blah", int>> my_widget;
```
