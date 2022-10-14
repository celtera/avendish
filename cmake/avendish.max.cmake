set(AVND_MAXSDK_PATH "" CACHE PATH "Path to Max SDK folder")
if(NOT AVND_MAXSDK_PATH)
  function(avnd_make_max)
  endfunction()

  return()
endif()

set(MAXSDK_MAX_INCLUDE_DIR "${AVND_MAXSDK_PATH}/source/c74support/max-includes")
set(MAXSDK_MSP_INCLUDE_DIR "${AVND_MAXSDK_PATH}/source/c74support/msp-includes")

if(APPLE)
  find_library(MAXSDK_API_LIBRARY NAMES MaxAPI HINTS "${MAXSDK_MAX_INCLUDE_DIR}")
elseif(WIN32)
  if("${CMAKE_SIZEOF_VOID_P}" MATCHES "4")
    find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}")
  else()
    find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}/x64")
  endif()
else()
  # add_library(MaxAPI INTERFACE)
  # add_library(MaxJitAPI INTERFACE)
  # set(MAXSDK_API_LIBRARY MaxAPI)
endif()

set(MAXSDK_MAX_INCLUDE_DIR  "${MAXSDK_MAX_INCLUDE_DIR}" CACHE INTERNAL "MAXSDK_MAX_INCLUDE_DIR")
set(MAXSDK_MSP_INCLUDE_DIR  "${MAXSDK_MSP_INCLUDE_DIR}" CACHE INTERNAL "MAXSDK_MSP_INCLUDE_DIR")
set(MAXSDK_API_LIBRARY  "${MAXSDK_API_LIBRARY}" CACHE INTERNAL "MAXSDK_API_LIBRARY")

function(avnd_make_max)
  if(NOT MAXSDK_API_LIBRARY)
    return()
  endif()

  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/max/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_max.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  set(AVND_FX_TARGET "${AVND_TARGET}_max")
  add_library(${AVND_FX_TARGET} MODULE)

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY max
      RUNTIME_OUTPUT_DIRECTORY max
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_max.cpp"
      "${MAXSDK_MAX_INCLUDE_DIR}/common/commonsyms.c"
  )

  target_compile_definitions(
    ${AVND_FX_TARGET}
    PRIVATE
      AVND_MAXMSP=1
  )

  target_include_directories(${AVND_FX_TARGET} PRIVATE
      "${MAXSDK_MAX_INCLUDE_DIR}"
      "${MAXSDK_MSP_INCLUDE_DIR}"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish_max
      DisableExceptions
  )

  if(APPLE)
    find_library(MAXSDK_API_LIBRARY NAMES MaxAPI HINTS "${MAXSDK_MAX_INCLUDE_DIR}")
    find_library(MAXSDK_MSP_LIBRARY NAMES MaxAudioAPI HINTS "${MAXSDK_MSP_INCLUDE_DIR}")

    target_compile_definitions(${AVND_FX_TARGET} PRIVATE MAC_VERSION)
    set_property(TARGET ${AVND_FX_TARGET} PROPERTY BUNDLE True)
    set_property(TARGET ${AVND_FX_TARGET} PROPERTY BUNDLE_EXTENSION "mxo")
    target_link_libraries(${AVND_FX_TARGET} PRIVATE -Wl,-undefined,dynamic_lookup)
    file(COPY "${AVND_SOURCE_DIR}/include/avnd/binding/max/resources/PkgInfo" DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/max/${AVND_C_NAME}.mxo/Contents/)
  elseif(WIN32)
    target_compile_definitions(${AVND_FX_TARGET} PRIVATE WIN_VERSION _CRT_SECURE_NO_WARNINGS)
    if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8")
        set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".mxe64")
        find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}/x64")
        find_library(MAXSDK_MSP_LIBRARY NAMES MaxAudio.lib HINTS "${MAXSDK_MSP_INCLUDE_DIR}/x64")
    else()
        set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".mxe")
        find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}")
        find_library(MAXSDK_MSP_LIBRARY NAMES MaxAudio.lib HINTS "${MAXSDK_MSP_INCLUDE_DIR}")
    endif()
  endif()

  target_link_libraries(${AVND_FX_TARGET} PRIVATE ${MAXSDK_API_LIBRARY} ${MAXSDK_MSP_LIBRARY})

  # We only export ext_main to prevent conflicts in e.g. Max4Live.
  #if(APPLE)
  #  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${AVND_C_NAME}-symbols.txt" "_ext_main\n")
  #  target_link_libraries(${AVND_FX_TARGET} PRIVATE "-Wl,-s,-exported_symbols_list,'${CMAKE_CURRENT_BINARY_DIR}/${AVND_C_NAME}-symbols.txt'")
  #endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_max INTERFACE)
target_link_libraries(Avendish_max INTERFACE Avendish)
add_library(Avendish::Avendish_max ALIAS Avendish_max)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/atom_iterator.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/audio_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/dsp.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/init.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/message_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/inputs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/messages.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/outputs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/max/helpers.hpp"
)
