find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG)
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

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "py${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY python
      RUNTIME_OUTPUT_DIRECTORY python
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
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
