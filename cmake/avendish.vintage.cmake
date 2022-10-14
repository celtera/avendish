
# Define a PCH
add_library(Avendish_vintage_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_precompile_headers(Avendish_vintage_pch
  PUBLIC
    include/avnd/binding/vintage/all.hpp
    include/avnd/prefix.hpp
)

target_link_libraries(Avendish_vintage_pch
  PUBLIC
    DisableExceptions
)
avnd_common_setup("" "Avendish_vintage_pch")

function(avnd_make_vintage)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})
  set(AVND_FX_TARGET "${AVND_TARGET}_vintage")
  add_library(${AVND_FX_TARGET} MODULE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_vintage.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_vintage.cpp"
  )

  target_precompile_headers(${AVND_FX_TARGET}
    REUSE_FROM
      Avendish_vintage_pch
  )

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}.vintage"
      LIBRARY_OUTPUT_DIRECTORY vintage
      RUNTIME_OUTPUT_DIRECTORY vintage
      VS_GLOBAL_IgnoreImportLibrary true
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish_vintage
      DisableExceptions
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_vintage INTERFACE)
target_link_libraries(Avendish_vintage INTERFACE Avendish)
add_library(Avendish::Avendish_vintage ALIAS Avendish_vintage)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/audio_effect.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/atomic_controls.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/dispatch.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/helpers.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/midi_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/processor_setup.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/programs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/vintage.hpp"
)
