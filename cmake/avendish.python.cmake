# Skip this back-end's setup (and its dependencies) entirely when it is disabled,
# so a single-back-end build -- e.g. one avnd-addon CI lane that passes
# -DAVND_ENABLE_PYTHON=OFF -- doesn't fetch/build what it won't use (godot-cpp, the
# VST3 SDK, ...). Matches _avnd_dispatch_backend: only act when explicitly OFF.
if(DEFINED AVND_ENABLE_PYTHON AND NOT AVND_ENABLE_PYTHON)
  return()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "WAS.*")
  function(avnd_make_python)
  endfunction()
  return()
endif()

# Opt-in stable-ABI build: one module that loads on any CPython >= the version
# below (Py_LIMITED_API / python3.dll) instead of a version-locked build that
# only imports on the exact interpreter it was compiled against (python3XY.dll).
#
# OFF by default: pybind11 uses the full CPython C API (PyFrameObject /
# PyCodeObject internals, PyList_GET_ITEM, ...) and does not compile under
# Py_LIMITED_API, so the stable-ABI path only works with a limited-API-capable
# binding. When on and available it is wired correctly (links python3.lib); the
# default keeps the working version-specific build. Build the module against the
# Python version you ship for.
option(AVND_PYTHON_STABLE_ABI "Build Python modules against the stable ABI (needs a limited-API-capable binding; pybind11 is not)" OFF)
set(AVND_PYTHON_SABI_VERSION "3.12" CACHE STRING "Minimum CPython version for the stable-ABI build (Python_add_library USE_SABI value)")

# Development.Module (not full Development) is what a loadable extension needs.
# Development.SABIModule (CMake >= 3.26) additionally provides the stable-ABI
# import library that Python_add_library(USE_SABI) links against; without it
# USE_SABI silently falls back to the version-specific library.
find_package(Python COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule GLOBAL)
if(NOT Python_Development.Module_FOUND)
  find_package(Python COMPONENTS Interpreter Development GLOBAL)
endif()
find_package(pybind11 CONFIG GLOBAL)
if(NOT TARGET pybind11::headers)
  function(avnd_make_python)
  endfunction()

  return()
endif()
# A true stable-ABI build needs Python_add_library(USE_SABI ...) so the module
# links the stable import library (python3.lib -> python3.dll) rather than the
# version-specific one; that command is available from CMake 3.26.
# Stored in the cache so avnd_make_python() sees it: that function is called from
# the consumer's scope, which the plain variable set here does not reach.
set(_avnd_python_can_sabi FALSE CACHE INTERNAL "")
if(AVND_PYTHON_STABLE_ABI AND Python_Development.SABIModule_FOUND
   AND NOT CMAKE_VERSION VERSION_LESS "3.26")
  set(_avnd_python_can_sabi TRUE CACHE INTERNAL "")
endif()

function(avnd_make_python)
  if(NOT TARGET pybind11::headers)
    return()
  endif()

  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME;EXAMPLE_PY" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/python/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_python.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  set(AVND_FX_TARGET "${AVND_TARGET}_python")
  set(_srcs
    "${AVND_MAIN_FILE}"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_python.cpp")

  if(_avnd_python_can_sabi)
    # WITH_SOABI stamps the extension suffix (.abi3.pyd / .abi3.so); USE_SABI
    # compiles with Py_LIMITED_API and links the stable import lib. The result
    # imports on any CPython >= AVND_PYTHON_SABI_VERSION.
    Python_add_library(${AVND_FX_TARGET} MODULE WITH_SOABI USE_SABI "${AVND_PYTHON_SABI_VERSION}"
      ${_srcs})
    set_target_properties(${AVND_FX_TARGET} PROPERTIES
      OUTPUT_NAME "py${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY python
      RUNTIME_OUTPUT_DIRECTORY python)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE Avendish::Avendish_python pybind11::headers)
  else()
    # Classic version-specific module (imports python3XY.dll). Used when the
    # stable ABI is disabled or unavailable (CMake < 3.26 / no Development.Module).
    add_library(${AVND_FX_TARGET} SHARED ${_srcs})
    if(WIN32)
      set(_pysuffix ".pyd")
    else()
      # CPython convention: py<name>.so on both Linux and macOS, no lib prefix.
      set(_pysuffix ".so")
    endif()
    set_target_properties(${AVND_FX_TARGET} PROPERTIES
      OUTPUT_NAME "py${AVND_C_NAME}"
      PREFIX ""
      SUFFIX "${_pysuffix}"
      LIBRARY_OUTPUT_DIRECTORY python
      RUNTIME_OUTPUT_DIRECTORY python)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE Avendish::Avendish_python pybind11::module)
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  # Register for avnd_addon_package() (the addon-wide packaging orchestrator).
  set_property(GLOBAL APPEND PROPERTY AVND_ALL_PYTHON_EXTERNALS ${AVND_FX_TARGET})

  # Usage example next to the module.
  avnd_generate_help(
    FX_TARGET     "${AVND_FX_TARGET}"
    SOURCE_TARGET "${AVND_TARGET}"
    BACKEND       python
    DESTINATION   "python/examples/$<IF:${multi_config},$<CONFIG>/,>${AVND_C_NAME}_example.py"
    PROPERTY      AVND_PYTHON_EXAMPLE
    OVERRIDE      "${AVND_EXAMPLE_PY}"
  )
endfunction()

add_library(Avendish_python INTERFACE)
target_link_libraries(Avendish_python INTERFACE Avendish)
add_library(Avendish::Avendish_python ALIAS Avendish_python)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/python/processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/python/configure.hpp"
)
