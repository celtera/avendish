get_cmake_property (multi_config GENERATOR_IS_MULTI_CONFIG)
set(multi_config "${multi_config}" CACHE INTERNAL "")
file(GENERATE
    OUTPUT "$<IF:${multi_config},$<CONFIG>/,>maxref_template.xml"
    INPUT ${AVND_SOURCE_DIR}/examples/Demos/maxref_template.xml)

function(avnd_make_dump)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  set(AVND_FX_TARGET "${AVND_TARGET}_dump")
  if(TARGET "${AVND_FX_TARGET}")
    # Target has already been created in avendish.cmake
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/dump/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_dump.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  add_executable(${AVND_FX_TARGET})

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY dump
      RUNTIME_OUTPUT_DIRECTORY dump
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_dump.cpp"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PRIVATE
      Avendish::Avendish_dump nlohmann_json::nlohmann_json
  )

  set(_dump_file_path "dump/$<IF:${multi_config},$<CONFIG>/,>${AVND_TARGET}.json")
  add_custom_command(
    DEPENDS ${AVND_FX_TARGET}
    COMMAND ${AVND_FX_TARGET} "${_dump_file_path}"
    OUTPUT  "${_dump_file_path}"
    VERBATIM
  )

  set_target_properties(${AVND_TARGET} PROPERTIES
    AVND_DUMP_PATH "${_dump_file_path}"
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_dump INTERFACE)
target_link_libraries(Avendish_dump INTERFACE Avendish)
add_library(Avendish::Avendish_dump ALIAS Avendish_dump)

