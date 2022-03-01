find_package(ossia QUIET)

if(NOT TARGET ossia::ossia)
  message(STATUS "libossia not found, skipping bindings...")

  function(avnd_make_ossia)
  endfunction()
  return()
endif()

# Define a PCH
add_library(Avendish_ossia_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")
target_link_libraries(Avendish_ossia_pch PRIVATE
  ossia::ossia
)

target_precompile_headers(Avendish_ossia_pch
  PUBLIC
    <ossia/prefix.hpp>
    include/avnd/binding/ossia/all.hpp
    include/avnd/prefix.hpp
)
avnd_common_setup("" "Avendish_ossia_pch")

# Function that can be used to wrap an object as an ossia node
function(avnd_make_ossia)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS" "" ${ARGN})

  set(AVND_FX_TARGET "${AVND_TARGET}_ossia")
  if(TARGET "${AVND_FX_TARGET}")
    # Target has already been created in avendish.cmake
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_ossia.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  add_library(${AVND_FX_TARGET} STATIC)

  set_target_properties(${AVND_FX_TARGET}
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ossia
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_ossia.cpp"
  )

  target_precompile_headers(${AVND_FX_TARGET}
    REUSE_FROM
      Avendish_ossia_pch
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish
      ossia::ossia
      SDL2
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  target_sources(Avendish PRIVATE
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/all.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/configure.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/node.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/message_node.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/mono_arg_node.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/poly_arg_node.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/port_node.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ossia/sample_arg_node.hpp"
  )
endfunction()
