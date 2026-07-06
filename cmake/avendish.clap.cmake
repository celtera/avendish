# Skip this back-end's setup (and its dependencies) entirely when it is disabled,
# so a single-back-end build -- e.g. one avnd-addon CI lane that passes
# -DAVND_ENABLE_CLAP=OFF -- doesn't fetch/build what it won't use (godot-cpp, the
# VST3 SDK, ...). Matches _avnd_dispatch_backend: only act when explicitly OFF.
if(DEFINED AVND_ENABLE_CLAP AND NOT AVND_ENABLE_CLAP)
  return()
endif()

find_path(CLAP_HEADER NAMES clap/clap.h)
if(NOT CLAP_HEADER)
  # The CLAP "SDK" is a set of plain C headers: fetch them when absent.
  include(FetchContent)
  FetchContent_Declare(
    avnd_clap_headers
    GIT_REPOSITORY "https://github.com/free-audio/clap"
    GIT_TAG 1.2.6
    GIT_PROGRESS true
  )
  FetchContent_GetProperties(avnd_clap_headers)
  if(NOT avnd_clap_headers_POPULATED)
    FetchContent_Populate(avnd_clap_headers)
  endif()
  if(EXISTS "${avnd_clap_headers_SOURCE_DIR}/include/clap/clap.h")
    set(CLAP_HEADER "${avnd_clap_headers_SOURCE_DIR}/include" CACHE PATH "clap headers" FORCE)
  endif()
endif()

if(NOT CLAP_HEADER)
  message(STATUS "Clap not found, skipping bindings...")

  function(avnd_make_clap)
  endfunction()
  return()
endif()

if(AVND_USE_PCH)
# Define a PCH
add_library(Avendish_clap_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_include_directories(
  Avendish_clap_pch
  PRIVATE
    ${CLAP_HEADER}
)

target_link_libraries(Avendish_clap_pch
  PUBLIC
    concurrentqueue
    DisableExceptions
)

target_precompile_headers(Avendish_clap_pch
  PUBLIC
    include/avnd/binding/clap/all.hpp
    include/avnd/prefix.hpp
)
avnd_common_setup("" "Avendish_clap_pch")
endif()

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
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_clap.cpp"
  )

  if(APPLE)
    # macOS: produce a .clap bundle: <C_NAME>.clap/Contents/MacOS/<C_NAME>
    set(_avnd_clap_plist "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_clap_Info.plist")
    configure_file(
      "${AVND_SOURCE_DIR}/include/avnd/binding/clap/Info.plist.in"
      "${_avnd_clap_plist}"
      @ONLY
    )
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "${AVND_C_NAME}"
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
        MACOSX_BUNDLE_INFO_PLIST "${_avnd_clap_plist}"
        MACOSX_BUNDLE_BUNDLE_NAME "${AVND_C_NAME}"
        XCODE_ATTRIBUTE_MACH_O_TYPE mh_bundle
        LIBRARY_OUTPUT_DIRECTORY clap
        RUNTIME_OUTPUT_DIRECTORY clap
        ARCHIVE_OUTPUT_DIRECTORY clap
        VS_GLOBAL_IgnoreImportLibrary true
    )
  else()
    # Linux / Windows: the shared module file IS the .clap
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "${AVND_C_NAME}.clap"
        PREFIX ""
        SUFFIX ""
        LIBRARY_OUTPUT_DIRECTORY clap
        RUNTIME_OUTPUT_DIRECTORY clap
        ARCHIVE_OUTPUT_DIRECTORY clap
        VS_GLOBAL_IgnoreImportLibrary true
    )
  endif()

  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      ${CLAP_HEADER}
  )

  if(AVND_USE_PCH)
    target_precompile_headers(${AVND_FX_TARGET}
      REUSE_FROM
        Avendish_clap_pch
    )
  endif()

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish_clap
      concurrentqueue
      DisableExceptions
  )

  # Editors: plug-ins with a `struct ui` get the reference soft UI editor
  # (pugl + Nuklear + canvas_ity); UI-less plug-ins are unaffected.
  if(TARGET Avendish::soft_ui AND TARGET avnd_pugl)
    avnd_target_soft_ui(${AVND_FX_TARGET})
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_clap INTERFACE)
target_link_libraries(Avendish_clap INTERFACE Avendish concurrentqueue)
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
