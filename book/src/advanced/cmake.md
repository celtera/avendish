# Configuration with CMake

So far, the "building" side of things has been left under the carpet.

It is actually not very complicated.

We have:

- A processor, `MyProcessor`.
- A binding for which we want to build this processor, for instance a Python or VST3 binding.

What CMake does is that it generates a small `.cpp` file that combines both.

Here is for instance how it is done for Python: 


```cpp
{{ #include ../../../include/avnd/binding/python/prototype.cpp.in }}
```

Here, `AVND_MAIN_FILE`, `AVND_C_NAME` and `AVND_MAIN_CLASS` are options that are passed to CMake. 
For an actual processor though, it's likely that you would have to write your own entrypoint.

Here is the Clap entrypoint, which is fairly similar: 

```cpp
{{ #include ../../../include/avnd/binding/clap/prototype.cpp.in }}
```

## CMake functions

The CMake script currently provides pre-made integrations with the bindings we support.

There are helper functions that build every binding possible in one go:

```cmake
# Create bindings for everything under the sun
avnd_make_all(...)

# Create bindings for general object-based systems: 
# - PureData, Max/MSP
# - Python
# - Ossia
# - Standalone demo examples
avnd_make_object(...)

# Create bindings for audio APIs: Vintage, VST3, Clap
avnd_make_audioplug(...)
```

Which just call the individual functions:

```cmake
avnd_make_vst3(...)
avnd_make_pd(...)
avnd_make_max(...)
etc...
```

These functions all have the same syntax:

```
avnd_make_all(
  # The CMake target:
  TARGET PerSampleProcessor2

  # The file to include to get a complete definition of the processor
  MAIN_FILE examples/Raw/PerSampleProcessor2.hpp

  # The C++ class name
  MAIN_CLASS examples::PerSampleProcessor2

  # A name to give for systems which depend on C-ish names for externals, like Max/MSP and PureData.
  C_NAME avnd_persample_2
)
```

## Doing it by hand

This is not very hard: Avendish is a header-only library, so you just have to add the `avendish/include` folder to your include path, 
and the `-std=c++20` flag to your build-system.

> Depending on your compiler, you may also need to add flags such as `-fconcepts` (GCC <= 9) ; `-fcoroutines` (GCC <= 11) ; `-fcoroutines-ts` (Clang <= 14).

> Until the reflection TS gets merged, we have a dependency on Boost.PFR so you also need to include `boost`. Boost.PFR is header-only.

> You also likely want to add `fmt` to get nice logging.

Finally, we have to wrap our class with the binding.

```cpp
// Defines struct MyProcessor { ... };
#include "MyProcessor.hpp"
#include "MyBinding.hpp"
// Set up those typedefs to provide services to plug-ins which need it
struct my_config {
   using logger_type = ...;
   using fft_1d_type = ...;
};

int main()
{
  // This will instantiate my_processor with the configuration passed as template argument if needed:
  using plug_type = decltype(avnd::configure<my_config, my_processor>())::type;

  // Finally create the binding object
  MyBinding<plug_type> the_binding;
}
```