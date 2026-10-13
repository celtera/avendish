find_path(CSOUND_HEADER NAMES csdl.h PATH_SUFFIXES csound)
if(NOT CSOUND_HEADER)
  message(STATUS "Csound headers (csdl.h) not found, skipping bindings...")
  function(avnd_make_csound)
  endfunction()

  return()
endif()

# Shared PCH, reused by every per-object external (mirrors Avendish_pd_pch).
add_library(Avendish_csound_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_include_directories(
  Avendish_csound_pch
  PRIVATE
    ${CSOUND_HEADER}
)

target_precompile_headers(Avendish_csound_pch
  PUBLIC
    include/avnd/binding/csound/all.hpp
    include/avnd/prefix.hpp
)

target_link_libraries(Avendish_csound_pch
  PUBLIC
    DisableExceptions
)

avnd_common_setup("" "Avendish_csound_pch")

function(avnd_make_csound)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/csound/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_csound.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  if(APPLE)
    set_source_files_properties("${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_csound.cpp" PROPERTIES COMPILE_FLAGS -Wno-unreachable-code)
  endif()

  set(AVND_FX_TARGET "${AVND_TARGET}_csound")
  add_library(${AVND_FX_TARGET} MODULE)

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY csound
      RUNTIME_OUTPUT_DIRECTORY csound
      ARCHIVE_OUTPUT_DIRECTORY csound
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_csound.cpp"
  )

  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CSOUND_HEADER}"
  )

  target_compile_definitions(
    ${AVND_FX_TARGET}
    PRIVATE
      AVND_CSOUND=1
  )

  if(NOT MSVC)
    target_precompile_headers(${AVND_FX_TARGET}
      REUSE_FROM
        Avendish_csound_pch
    )
  endif()

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      DisableExceptions
      Avendish::Avendish_csound
  )

  # Opcode plugins resolve the API through the CSOUND* function table at load
  # time, so the engine symbols are deliberately left undefined in the library.
  if(APPLE)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE -Wl,-undefined,dynamic_lookup)
  elseif(UNIX)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE -Wl,--allow-shlib-undefined)
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_csound INTERFACE)
target_link_libraries(Avendish_csound INTERFACE Avendish)
target_include_directories(Avendish_csound INTERFACE "${CSOUND_HEADER}")
add_library(Avendish::Avendish_csound ALIAS Avendish_csound)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/all.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/audio_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/helpers.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/inputs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/message_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/outputs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/csound/typestrings.hpp"
)
