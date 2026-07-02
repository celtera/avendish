# Golden-output generator backend.
#
# Mirrors avendish.dump: for each object we build a small executable that
# instantiates the raw C++ object, feeds it deterministic test input, runs its
# processing, and writes golden/<c_name>.json (inputs + outputs + meta). That
# file is the oracle every backend binding must reproduce (see
# OUTPUT_VERIFICATION_PLAN.md).

function(avnd_make_golden)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  set(AVND_FX_TARGET "${AVND_TARGET}_golden")
  if(TARGET "${AVND_FX_TARGET}")
    # Target has already been created in avendish.cmake
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/golden/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_golden.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  add_executable(${AVND_FX_TARGET})

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY golden
      RUNTIME_OUTPUT_DIRECTORY golden
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_golden.cpp"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PRIVATE
      Avendish::Avendish_golden yyjson
  )

  # Reuse the shared PCH when it exists (only in avendish's own builds).
  if(NOT MSVC AND TARGET Avendish_golden_pch)
    target_precompile_headers(${AVND_FX_TARGET}
      REUSE_FROM
        Avendish_golden_pch
    )
  endif()

  # Running the object can fail (GPU/file/network objects) -- keep it
  # best-effort so it never breaks the build; the exe writes a "skip" record
  # on failure and `|| true` swallows a hard crash.
  set(_golden_file_path "golden/$<IF:${multi_config},$<CONFIG>/,>${AVND_TARGET}.json")
  add_custom_command(
    DEPENDS ${AVND_FX_TARGET}
    COMMAND ${AVND_FX_TARGET} "${_golden_file_path}" || ${CMAKE_COMMAND} -E true
    OUTPUT  "${_golden_file_path}"
    VERBATIM
  )

  set_target_properties(${AVND_TARGET} PROPERTIES
    AVND_GOLDEN_PATH "${_golden_file_path}"
  )
  set_property(GLOBAL APPEND PROPERTY AVND_GOLDEN_OUTPUTS "${_golden_file_path}")

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

# After all objects are registered, wire a `goldens` aggregate target that runs
# every golden generator (each produces golden/<c_name>.json).
function(_avnd_finalize_goldens)
  get_property(_outs GLOBAL PROPERTY AVND_GOLDEN_OUTPUTS)
  if(_outs AND NOT TARGET goldens)
    add_custom_target(goldens DEPENDS ${_outs})
  endif()
endfunction()
if(CMAKE_SOURCE_DIR STREQUAL AVND_SOURCE_DIR)
  cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _avnd_finalize_goldens)
endif()

add_library(Avendish_golden INTERFACE)
target_link_libraries(Avendish_golden INTERFACE Avendish yyjson)
add_library(Avendish::Avendish_golden ALIAS Avendish_golden)

# Shared PCH for all golden executables (same heavy headers as the dump PCH).
if(NOT MSVC AND CMAKE_SOURCE_DIR STREQUAL AVND_SOURCE_DIR)
  add_library(Avendish_golden_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")
  target_link_libraries(Avendish_golden_pch
    PUBLIC
      Avendish::Avendish_golden
  )
  target_precompile_headers(Avendish_golden_pch
    PUBLIC
      <vector>
      <map>
      <string>
      <string_view>
      <variant>
      <optional>
      <tuple>
      <cmath>
      <avnd/binding/dump/json_writer.hpp>
      <halp/log.hpp>
  )
  avnd_common_setup("" "Avendish_golden_pch")
endif()
