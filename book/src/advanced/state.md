# Host state (session persistence)

Processors sometimes carry state that is not expressible as parameter ports:
pattern data, captured audio, generated content... By exposing a pair of
member functions, that state is persisted in the DAW session by the plug-in
bindings (currently CLAP and VST3), alongside the usual parameter values:

```cpp
struct MyProcessor
{
  ...

  std::vector<uint8_t> save_state();
  bool load_state(const uint8_t* data, size_t n);
};
```

Several spellings are accepted (see `<avnd/concepts/state.hpp>`):

- `save_state()` may return any contiguous byte container
  (`std::vector<uint8_t>`, `std::string`, `std::vector<std::byte>`, ...);
- alternatively, a writer callback form allows streaming without allocation:

```cpp
void save_state(avnd::state_writer auto& write)
{
  write(header_bytes, sizeof(header_bytes));
  write(pattern.data(), pattern.size());
}
```

- `load_state` may take `(const uint8_t*, size_t)`, `(const char*, size_t)`,
  `(const std::byte*, size_t)` or a `std::span<const uint8_t>`; returning
  `bool` allows rejecting an invalid blob (`void` also works and means
  loading cannot fail).

Important notes:

- The blob is **owned by the processor**: version it, validate it (it comes
  back from arbitrary session files), and return `false` on anything
  suspicious **without partially applying it**. The binding then keeps the
  current state and reports the failure to the host.
- Parameters are restored first, then `load_state` runs: a processor that
  reconfigures itself from the blob wins where the two overlap.
- Save and load run on the host's state-handling thread (main/UI thread in
  practice), like parameter restoration.

A complete example: `examples/Raw/CustomState.hpp`.
