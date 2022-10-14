find_path(CLAP_HEADER NAMES clap/clap.h)
if(NOT CLAP_HEADER)
  message(STATUS "Clap not found, skipping bindings...")

  function(avnd_make_clap)
  endfunction()
  return()
endif()

# Define a PCH
add_library(Avendish_clap_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_include_directories(
  Avendish_clap_pch
  PRIVATE
    ${CLAP_HEADER}
)

target_link_libraries(Avendish_clap_pch
  PUBLIC
    DisableExceptions
)

target_precompile_headers(Avendish_clap_pch
  PUBLIC
    include/avnd/binding/clap/all.hpp
    include/avnd/prefix.hpp
)
avnd_common_setup("" "Avendish_clap_pch")

# Function that can be used to wrap an object
function(avnd_make_clap)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})
  set(AVND_FX_TARGET "${AVND_TARGET}_clap")
  add_library(${AVND_FX_TARGET} MODULE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/clap/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_clap.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_clap.cpp"
  )

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}.clap"
      LIBRARY_OUTPUT_DIRECTORY clap
      RUNTIME_OUTPUT_DIRECTORY clap
      VS_GLOBAL_IgnoreImportLibrary true
  )

  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      ${CLAP_HEADER}
  )

  target_precompile_headers(${AVND_FX_TARGET}
    REUSE_FROM
      Avendish_clap_pch
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish_clap
      DisableExceptions
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_clap INTERFACE)
target_link_libraries(Avendish_clap INTERFACE Avendish)
add_library(Avendish::Avendish_clap ALIAS Avendish_clap)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/clap/audio_effect.hpp"
  #"${AVND_SOURCE_DIR}/include/avnd/binding/clap/atomic_controls.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/clap/configure.hpp"
  #"${AVND_SOURCE_DIR}/include/avnd/binding/clap/dispatch.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/clap/helpers.hpp"
  #"${AVND_SOURCE_DIR}/include/avnd/binding/clap/midi_processor.hpp"
  #"${AVND_SOURCE_DIR}/include/avnd/binding/clap/processor_setup.hpp"
  #"${AVND_SOURCE_DIR}/include/avnd/binding/clap/programs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/clap/clap.hpp"
)
