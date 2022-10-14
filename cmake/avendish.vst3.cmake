set(VST3_SDK_ROOT "" CACHE PATH "VST3 SDK path")
if(NOT VST3_SDK_ROOT)
  function(avnd_make_vst3)
  endfunction()

  return()
endif()

if(WIN32)
  # Needed because on windows we need admin permissions which does not work on CI
  # (see smtg_create_directory_as_admin_win)
  set(SMTG_PLUGIN_TARGET_PATH "${CMAKE_CURRENT_BINARY_DIR}/vst3_path" CACHE PATH "vst3 folder")
  file(MAKE_DIRECTORY "${SMTG_PLUGIN_TARGET_PATH}")
endif()

# https://forums.steinberg.net/t/pluginterfaces-lib-compilation-error-win-10-vs-2022/768976/3
if(MSVC)
  set(SMTG_USE_STDATOMIC_H OFF)
endif()

set(SMTG_ADD_VST3_HOSTING_SAMPLES 0)
set(SMTG_ADD_VST3_HOSTING_SAMPLES 0 CACHE INTERNAL "")

add_definitions(-DDEVELOPMENT)
include_directories("${VST3_SDK_ROOT}")

# VST3 uses COM APIs which require no virtual dtors in interfaces
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-non-virtual-dtor")
endif()

add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3_sdk")

function(avnd_make_vst3)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})
  set(AVND_FX_TARGET "${AVND_TARGET}_vst3")
  add_library(${AVND_FX_TARGET} MODULE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_vst3.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_vst3.cpp"
  )

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY "vst3/${AVND_C_NAME}.vst3/Contents/x86_64-linux"
      RUNTIME_OUTPUT_DIRECTORY "vst3/${AVND_C_NAME}.vst3/Contents/x86_64-linux"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish_vst3
      sdk_common pluginterfaces
      DisableExceptions
  )
  if(APPLE)
    find_library(COREFOUNDATION_FK CoreFoundation)
    target_link_libraries(
      ${AVND_FX_TARGET}
      PRIVATE
        ${COREFOUNDATION_FK}
    )
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_vst3 INTERFACE)
target_link_libraries(Avendish_vst3 INTERFACE Avendish)
add_library(Avendish::Avendish_vst3 ALIAS Avendish_vst3)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/audio_effect.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/bus_info.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/component.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/component_base.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/connection_point.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/controller.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/controller_base.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/factory.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/helpers.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/metadata.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/programs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/vst3/refcount.hpp"
)
