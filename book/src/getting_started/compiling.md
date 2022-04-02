# Compiling our processor

## Environment set-up
Before anything, we need a C++ compiler. The recommandation is to use Clang (at least clang-13). GCC 11 also works with some limitations. Visual Studio is sadly still not competent enough.

- On Windows, through [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases/tag/20220323). 
- On Mac, through Xcode.
- On Linux, through your distribution packages.

Avendish's code is header-only ; however, CMake automatizes correctly linking to the relevant libraries, and generates a correct entrypoint for the targeted bindings, thus we recommend installing it.

Ninja is recommended: it makes the build faster.

The APIs and SDK that you wish to create plug-ins / bindings for must also be available: 

- PureData: needs the PureData API.
  * m_pd.h and pd.lib must be findable through `CMAKE_PREFIX_PATH`.
  * On Linux this is automatic if you install PureData through your distribution.
- Max/MSP: needs the Max SDK.
  * Pass `-DAVND_MAXSDK_PATH=/path/to/max/sdk` to CMake.
- Python: needs pybind11.
  * Installable through most distro's repos.
- ossia: needs [libossia](https://github.com/ossia/libossia).
- clap: needs [clap](https://github.com/free-audio/clap).
- UIs can be built with Qt or [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear).
  * Qt is installable easily through [aqtinstall](https://github.com/miurahr/aqtinstall).
- VST3: needs the Steinberg VST3 SDK.
  * Pass `-DVST3_SDK_ROOT=/path/to/vst3/sdk` to CMake.
- By default, plug-ins compatible with most DAWs through an obsolete, Vintage, almost vestigial, API will be built. This does not require any specific dependency to be installed, on the other hand it only supports audio plug-ins.

## Building the template

The simplest way to get started is from the [template repository](https://github.com/celtera/avendish-audio-processor-template/blob/main/CMakeLists.txt): simply clear the [Processor.cpp](https://github.com/celtera/avendish-audio-processor-template/blob/main/src/Processor.cpp) file for now and put the content in [Processor.hpp](https://github.com/celtera/avendish-audio-processor-template/blob/main/src/Processor.hpp).

Here's a complete example (from bash):
```bash
$ git clone https://github.com/celtera/avendish-audio-processor-template
$ mkdir build
$ cd build
$ cmake ../avendish-audio-processor-template
$ ninja # or make -j8
```

This should produce various binaries in the build folder: for instance, a PureData object (in `build/pd`), a Python one (in `build/python`, etc.).
