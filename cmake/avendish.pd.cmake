find_path(PD_HEADER NAMES m_pd.h)
if(NOT PD_HEADER)
  message(STATUS "PureData header not found, skipping bindings...")
  function(avnd_make_pd)
  endfunction()

  return()
endif()

if(WIN32)
  find_library(PD_LIB NAMES pd)
  if(NOT PD_LIB)
    message(STATUS "PureData library not found, skipping bindings...")
    function(avnd_make_pd)
    endfunction()

    return()
  endif()
endif()

# Define a PCH
add_library(Avendish_pd_pch STATIC "${AVND_SOURCE_DIR}/src/dummy.cpp")

target_include_directories(
  Avendish_pd_pch
  PRIVATE
    ${PD_HEADER}
)

target_precompile_headers(Avendish_pd_pch
  PUBLIC
    include/avnd/binding/pd/all.hpp
    include/avnd/prefix.hpp
)

target_link_libraries(Avendish_pd_pch
  PUBLIC
    DisableExceptions
)

avnd_common_setup("" "Avendish_pd_pch")

function(avnd_make_pd)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/pd/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_pd.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  if(APPLE)
    set_source_files_properties("${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_pd.cpp" PROPERTIES COMPILE_FLAGS -Wno-unreachable-code)
  endif()

  set(AVND_FX_TARGET "${AVND_TARGET}_pd")
  add_library(${AVND_FX_TARGET} MODULE)

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY pd
      RUNTIME_OUTPUT_DIRECTORY pd
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_pd.cpp"
  )

  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      "${PD_HEADER}"
  )

  target_compile_definitions(
    ${AVND_FX_TARGET}
    PRIVATE
      AVND_PUREDATA=1
  )

  if(NOT MSVC)
    target_precompile_headers(${AVND_FX_TARGET}
      REUSE_FROM
        Avendish_pd_pch
    )
  endif()

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      DisableExceptions
    PUBLIC
      Avendish::Avendish_pd
  )

  if(APPLE)
    set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".pd_darwin")
    target_link_libraries(${AVND_FX_TARGET} PRIVATE -Wl,-undefined,dynamic_lookup)
  elseif(UNIX)
    if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm")
      set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".l_arm")
    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64")
      set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".l_ia64")
    elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86")
      set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".l_i386")
    else()
      set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".pd_linux")
    endif()
    target_link_libraries(${AVND_FX_TARGET} PRIVATE -Wl,--allow-shlib-undefined)
  elseif(WIN32)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE ${PD_LIB})
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_pd INTERFACE)
target_link_libraries(Avendish_pd INTERFACE Avendish)
add_library(Avendish::Avendish_pd ALIAS Avendish_pd)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/atom_iterator.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/audio_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/dsp.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/init.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/inputs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/message_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/messages.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/outputs.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/pd/helpers.hpp"
)

function(avnd_create_pd_package)
  cmake_parse_arguments(AVND
      "INSTALL"
      "NAME;SOURCE_PATH;PACKAGE_ROOT"
      "EXTERNALS;SUPPORT"
      ${ARGN})

  set(_pkg_target "${AVND_NAME}_pd_package")
  add_custom_target(${_pkg_target} ALL
    DEPENDS ${AVND_EXTERNALS}
  )

  # Copy the package base
  set(_pkg "${AVND_PACKAGE_ROOT}/${AVND_NAME}")
  file(GLOB _pkg_content
       LIST_DIRECTORIES true
       "${AVND_SOURCE_PATH}/*")
  foreach(f ${_pkg_content})
    file(COPY "${f}" DESTINATION "${_pkg}/")
  endforeach()

  # Copy the externals
  foreach(_external ${AVND_EXTERNALS})
    set(_external_bin "$<TARGET_FILE:${_external}>")
    set(_external_path $<PATH:GET_PARENT_PATH,${_external_bin}>)
    get_target_property(_c_name ${_external} AVND_C_NAME)

    # Copy the external (fairly platform-specific)
    add_custom_command(TARGET ${_external} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "${_external_bin}" "${_pkg}/"
    )
  endforeach()

  # Copy the support files
  foreach(_support ${AVND_SUPPORT})
    add_custom_command(TARGET ${_pkg_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "${_support}" "${_pkg}/"
    )
  endforeach()
endfunction()
