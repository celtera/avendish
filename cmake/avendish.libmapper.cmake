find_path(LIBMAPPER_INCLUDE_DIR
    mapper/mapper.h
  HINTS
    /opt/libmapper/include
)

find_library(LIBMAPPER_LIBRARIES mapper
    HINTS
      /opt/libmapper/lib
)

if(LIBMAPPER_INCLUDE_DIR AND LIBMAPPER_LIBRARIES)
  add_library(mapper INTERFACE)
  target_include_directories(mapper INTERFACE "${LIBMAPPER_INCLUDE_DIR}")
  target_link_libraries(mapper INTERFACE "${LIBMAPPER_LIBRARIES}")
endif()

if(NOT LIBMAPPER_INCLUDE_DIR)
  function(avnd_make_libmapper)
  endfunction()

  return()
endif()

function(avnd_make_libmapper)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/libmapper/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_libmapper.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  set(AVND_FX_TARGET "${AVND_TARGET}_libmapper")
  add_executable(${AVND_FX_TARGET})

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY libmapper
      RUNTIME_OUTPUT_DIRECTORY libmapper
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_libmapper.cpp"
  )

  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      "${LIBMAPPER_INCLUDE_DIR}"
  )

  target_compile_definitions(
    ${AVND_FX_TARGET}
    PRIVATE
      AVND_LIBMAPPER=1
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PRIVATE
      Avendish::Avendish_libmapper
      mapper
      DisableExceptions
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_libmapper INTERFACE)
target_link_libraries(Avendish_libmapper INTERFACE Avendish)
add_library(Avendish::Avendish_libmapper ALIAS Avendish_libmapper)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/libmapper/libmapper.hpp"
)
