find_package(PkgConfig)
if(NOT PKG_CONFIG_EXECUTABLE)
    function(avnd_make_gstreamer)
    endfunction()
    return()
endif()

pkg_check_modules(Gstreamer IMPORTED_TARGET GLOBAL
        gobject-2.0
        glib-2.0
        gstreamer-sdp-1.0
        gstreamer-pbutils-1.0
        libsoup-2.4
        json-glib-1.0
        gstreamer-check-1.0)

if(NOT Gstreamer_FOUND)
    function(avnd_make_gstreamer)
    endfunction()
    return()
endif()
# Define a PCH
add_library(Avendish_gstreamer_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_precompile_headers(Avendish_gstreamer_pch
  PUBLIC
    # include/avnd/binding/gstreamer/element.hpp
    include/avnd/prefix.hpp
)

target_link_libraries(Avendish_gstreamer_pch
  PUBLIC
    DisableExceptions
    PkgConfig::Gstreamer
)

avnd_common_setup("" "Avendish_gstreamer_pch")

function(avnd_make_gstreamer)
  cmake_parse_arguments(AVND "" "PROCESSOR_TYPE;TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}_${AVND_PROCESSOR_TYPE}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_gstreamer.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  set(AVND_FX_TARGET "${AVND_TARGET}_${AVND_PROCESSOR_TYPE}_gstreamer")
  if(TARGET "${AVND_FX_TARGET}")
    return()
  endif()
  add_library(${AVND_FX_TARGET} MODULE)

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY gstreamer
      RUNTIME_OUTPUT_DIRECTORY gstreamer
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/audio_source.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/audio_sink.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/audio_filter.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/element.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/parameters.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/texture_source.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/texture_sink.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/texture_filter.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/utils.hpp"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_gstreamer.cpp"
  )

  target_compile_definitions(
    ${AVND_FX_TARGET}
    PRIVATE
      AVND_GSTREAMER=1
  )

  if(NOT MSVC)
    target_precompile_headers(${AVND_FX_TARGET}
      REUSE_FROM
        Avendish_gstreamer_pch
    )
  endif()

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      DisableExceptions
    PUBLIC
      Avendish::Avendish_gstreamer
      PkgConfig::Gstreamer
  )

  target_link_libraries(${AVND_FX_TARGET} PRIVATE ${Gstreamer_LIBRARIES})
  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_gstreamer INTERFACE)
target_link_libraries(Avendish_gstreamer INTERFACE Avendish)
add_library(Avendish::Avendish_gstreamer ALIAS Avendish_gstreamer)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/gstreamer/element.hpp"
)
