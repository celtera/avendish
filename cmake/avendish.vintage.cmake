# Skip this back-end's setup (and its dependencies) entirely when it is disabled,
# so a single-back-end build -- e.g. one avnd-addon CI lane that passes
# -DAVND_ENABLE_VINTAGE=OFF -- doesn't fetch/build what it won't use (godot-cpp, the
# VST3 SDK, ...). Matches _avnd_dispatch_backend: only act when explicitly OFF.
if(DEFINED AVND_ENABLE_VINTAGE AND NOT AVND_ENABLE_VINTAGE)
  return()
endif()

if(AVND_USE_PCH)
# Define a PCH
add_library(Avendish_vintage_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_precompile_headers(Avendish_vintage_pch
  PUBLIC
    include/avnd/binding/vintage/all.hpp
    include/avnd/prefix.hpp
)

target_link_libraries(Avendish_vintage_pch
  PUBLIC
    concurrentqueue
    DisableExceptions
)
avnd_common_setup("" "Avendish_vintage_pch")
endif()

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
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_vintage.cpp"
  )

  if(AVND_USE_PCH)
    target_precompile_headers(${AVND_FX_TARGET}
      REUSE_FROM
        Avendish_vintage_pch
    )
  endif()

  if(APPLE)
    # macOS: produce a .vst bundle: <C_NAME>.vst/Contents/MacOS/<C_NAME>
    set(_avnd_vintage_plist "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_vintage_Info.plist")
    configure_file(
      "${AVND_SOURCE_DIR}/include/avnd/binding/vintage/Info.plist.in"
      "${_avnd_vintage_plist}"
      @ONLY
    )
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "${AVND_C_NAME}"
        BUNDLE TRUE
        BUNDLE_EXTENSION "vst"
        MACOSX_BUNDLE_INFO_PLIST "${_avnd_vintage_plist}"
        MACOSX_BUNDLE_BUNDLE_NAME "${AVND_C_NAME}"
        XCODE_ATTRIBUTE_MACH_O_TYPE mh_bundle
        LIBRARY_OUTPUT_DIRECTORY vintage
        RUNTIME_OUTPUT_DIRECTORY vintage
        ARCHIVE_OUTPUT_DIRECTORY vintage
        VS_GLOBAL_IgnoreImportLibrary true
    )
  elseif(WIN32)
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "${AVND_C_NAME}"
        PREFIX ""
        SUFFIX ".dll"
        LIBRARY_OUTPUT_DIRECTORY vintage
        RUNTIME_OUTPUT_DIRECTORY vintage
        ARCHIVE_OUTPUT_DIRECTORY vintage
        VS_GLOBAL_IgnoreImportLibrary true
    )
  else()
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "${AVND_C_NAME}"
        PREFIX ""
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY vintage
        RUNTIME_OUTPUT_DIRECTORY vintage
        ARCHIVE_OUTPUT_DIRECTORY vintage
        VS_GLOBAL_IgnoreImportLibrary true
    )
  endif()

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish_vintage
      concurrentqueue
      DisableExceptions
  )

  # Editors: plug-ins with a `struct ui` get the reference soft UI editor.
  if(TARGET Avendish::soft_ui AND TARGET avnd_pugl)
    avnd_target_soft_ui(${AVND_FX_TARGET})
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_vintage INTERFACE)
target_link_libraries(Avendish_vintage INTERFACE Avendish concurrentqueue)
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
