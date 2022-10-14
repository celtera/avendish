include(CTest)

function(avnd_make_example_host)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS" "" ${ARGN})

  set(AVND_FX_TARGET "${AVND_TARGET}_example_host")
  if(TARGET "${AVND_FX_TARGET}")
    # Target has already been created in avendish.cmake
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/example/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_example_host.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  add_executable(${AVND_FX_TARGET})
  add_test(NAME ${AVND_FX_TARGET} COMMAND ${AVND_FX_TARGET})

  set_target_properties(${AVND_FX_TARGET}
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY example
  )
  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_example_host.cpp"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish
      DisableExceptions
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  target_sources(Avendish PRIVATE
    "${AVND_SOURCE_DIR}/include/avnd/binding/example/example_processor.hpp"
  )
endfunction()
