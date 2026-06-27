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

find_package(Python COMPONENTS Interpreter Development GLOBAL)
find_package(pybind11 CONFIG GLOBAL)
if(NOT TARGET pybind11::module)
  function(avnd_make_python)
  endfunction()

  return()
endif()
function(avnd_make_python)
  if(NOT TARGET pybind11::module)
    return()
  endif()

  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/python/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_python.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  set(AVND_FX_TARGET "${AVND_TARGET}_python")
  add_library(${AVND_FX_TARGET} SHARED)

  if(WIN32)
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "py${AVND_C_NAME}"
        PREFIX ""
        SUFFIX ".pyd"
        LIBRARY_OUTPUT_DIRECTORY python
        RUNTIME_OUTPUT_DIRECTORY python
    )
  else()
    # CPython convention: py<name>.so on both Linux and macOS, no lib prefix.
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "py${AVND_C_NAME}"
        PREFIX ""
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY python
        RUNTIME_OUTPUT_DIRECTORY python
    )
  endif()

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_python.cpp"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PRIVATE
      Avendish::Avendish_python
      pybind11::module
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_python INTERFACE)
target_link_libraries(Avendish_python INTERFACE Avendish)
add_library(Avendish::Avendish_python ALIAS Avendish_python)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/python/processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/python/configure.hpp"
)
