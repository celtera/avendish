if(CMAKE_SYSTEM_NAME MATCHES "WAS.*")
  function(avnd_make_standalone)
  endfunction()
  return()
endif()
find_package(ossia QUIET)
find_package(GLEW QUIET)
find_package(glfw3 QUIET)
find_package(OpenGL QUIET)
if(NOT TARGET Qt5::Quick)
  if(NOT ((TARGET GLEW::GLEW) AND (TARGET glfw) AND (TARGET OpenGL::GL)))
    function(avnd_make_standalone)
    endfunction()
    return()
  endif()
endif()

add_library(Avendish_standalone_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_precompile_headers(Avendish_standalone_pch
  PUBLIC
    include/avnd/binding/standalone/all.hpp
    include/avnd/prefix.hpp
)

if(TARGET Qt5::Quick)
  target_link_libraries(Avendish_standalone_pch PRIVATE
    Qt5::Quick
  )

  target_precompile_headers(Avendish_standalone_pch
    PUBLIC
      <QObject>
      <QQuickItem>
  )
endif()

if(TARGET ossia::ossia)
  target_link_libraries(Avendish_standalone_pch PRIVATE
    ossia::ossia
  )

  target_precompile_headers(Avendish_standalone_pch
    PUBLIC
      <ossia/prefix.hpp>
  )
endif()

avnd_common_setup("" "Avendish_standalone_pch")

if(TARGET Qt5::Quick)
function(avnd_make_standalone)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/prototype.cpp.in"
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
      "${AVND_MAIN_FILE}"
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
        SDL2
    )
  endif()

  if(TARGET Qt5::Quick)
    target_link_libraries(
      ${AVND_FX_TARGET}
      PUBLIC
        Qt5::Quick
    )
  endif()

  if(TARGET GLEW::GLEW)
    target_link_libraries(
      ${AVND_FX_TARGET}
      PUBLIC
        GLEW::GLEW
        glfw
        OpenGL::GL
    )
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  target_sources("${AVND_FX_TARGET}" PRIVATE
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/audio.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/configure.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/standalone.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/oscquery_mapper.hpp"
  )
endfunction()
else()
function(avnd_make_standalone)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/prototype.cpp.in"
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
      ${AVENDISH_SOURCES}
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
        SDL2
    )
  endif()

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      GLEW::GLEW
      glfw
      OpenGL::GL
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  target_sources(${AVND_FX_TARGET} PRIVATE
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/audio.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/configure.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/standalone.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/standalone/oscquery_mapper.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ui/nuklear_layout_ui.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ui/nuklear/nuklear.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/binding/ui/nuklear/nuklear_glfw_gl4.h"
  )
endfunction()
endif()
