find_package(ossia QUIET)

function(avnd_make_standalone)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    include/avnd/binding/standalone/prototype.cpp.in
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_standalone.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  set(AVND_FX_TARGET "${AVND_TARGET}_standalone")
  add_executable(${AVND_FX_TARGET})

  set_target_properties(${AVND_FX_TARGET}
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY standalone
  )
  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_standalone.cpp"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish
  )

  if(TARGET ossia::ossia)
    target_link_libraries(
      ${AVND_FX_TARGET}
      PUBLIC
        ossia::ossia
    )
  endif()

  if(TARGET Qt5::Quick)
    target_link_libraries(
      ${AVND_FX_TARGET}
      PUBLIC
        Qt5::Quick
    )
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  target_sources(Avendish PRIVATE
    include/avnd/binding/standalone/audio.hpp
    include/avnd/binding/standalone/configure.hpp
    include/avnd/binding/standalone/standalone.hpp
    include/avnd/binding/standalone/oscquery_mapper.hpp
  )
endfunction()
