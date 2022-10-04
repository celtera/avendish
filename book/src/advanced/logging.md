# Logging feature

> Supported bindings: all

The API is modeled after [spdlog](https://github.com/gabime/spdlog) and expects the [fmt](https://github.com/fmtlib/fmt) syntax: 

```cpp
{{ #include ../../../examples/Helpers/Logger.hpp }}
```

> Avendish currently will look for `fmtlib` for its logger implementation, until `std::format` gets implemented by compiler authors.
