set(AVND_MAXSDK_PATH "" CACHE PATH "Path to Max SDK folder")
if(NOT AVND_MAXSDK_PATH)
  function(avnd_make_max)
  endfunction()

  return()
endif()

if(EXISTS "${AVND_MAXSDK_PATH}/source/c74support/max-includes")
  set(MAXSDK_MAX_INCLUDE_DIR "${AVND_MAXSDK_PATH}/source/c74support/max-includes")
  set(MAXSDK_MSP_INCLUDE_DIR "${AVND_MAXSDK_PATH}/source/c74support/msp-includes")
elseif(EXISTS "${AVND_MAXSDK_PATH}/c74support/max-includes")
  set(MAXSDK_MAX_INCLUDE_DIR "${AVND_MAXSDK_PATH}/c74support/max-includes")
  set(MAXSDK_MSP_INCLUDE_DIR "${AVND_MAXSDK_PATH}/c74support/msp-includes")
endif()

if(APPLE)
  find_library(MAXSDK_API_LIBRARY NAMES MaxAPI HINTS "${MAXSDK_MAX_INCLUDE_DIR}")
elseif(WIN32)
  if("${CMAKE_SIZEOF_VOID_P}" MATCHES "4")
    find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}")
  else()
    find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}/x64")
  endif()
endif()

file(READ "${MAXSDK_MAX_INCLUDE_DIR}/c74_linker_flags.txt" MAXSDK_LINKER_FLAGS)
string(STRIP "${MAXSDK_LINKER_FLAGS}" MAXSDK_LINKER_FLAGS)

set(MAXSDK_MAX_INCLUDE_DIR  "${MAXSDK_MAX_INCLUDE_DIR}" CACHE INTERNAL "MAXSDK_MAX_INCLUDE_DIR")
set(MAXSDK_MSP_INCLUDE_DIR  "${MAXSDK_MSP_INCLUDE_DIR}" CACHE INTERNAL "MAXSDK_MSP_INCLUDE_DIR")
set(MAXSDK_API_LIBRARY  "${MAXSDK_API_LIBRARY}" CACHE INTERNAL "MAXSDK_API_LIBRARY")
set(MAXSDK_LINKER_FLAGS  "${MAXSDK_LINKER_FLAGS}" CACHE INTERNAL "MAXSDK_LINKER_FLAGS")

# Commonsyms from max
add_library(maxmsp_commonsyms STATIC
    "${MAXSDK_MAX_INCLUDE_DIR}/common/commonsyms.c"
)
target_compile_definitions(
  maxmsp_commonsyms
  PRIVATE
    AVND_MAXMSP=1
    $<$<BOOL:${WIN32}>:MAXAPI_USE_MSCRT=1>
    C74_USE_STRICT_TYPES=1
)

target_include_directories(maxmsp_commonsyms PRIVATE
    "${MAXSDK_MAX_INCLUDE_DIR}"
    "${MAXSDK_MSP_INCLUDE_DIR}"
)

# We only want to export this on Mac
if(APPLE)
  file(CONFIGURE
    OUTPUT "${CMAKE_BINARY_DIR}/maxmsp-symbols.txt"
    CONTENT "_ext_main\n"
    NEWLINE_STYLE LF
  )
endif()

function(avnd_make_max)
  if(NOT MAXSDK_MAX_INCLUDE_DIR)
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

  if(APPLE)
    set_source_files_properties("${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_max.cpp" PROPERTIES COMPILE_FLAGS -Wno-unreachable-code)
  endif()

  set(AVND_FX_TARGET "${AVND_TARGET}_max")
  add_library(${AVND_FX_TARGET} MODULE)

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY max
      RUNTIME_OUTPUT_DIRECTORY max
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_max.cpp"

      "${AVND_SOURCE_DIR}/include/avnd/binding/max/all.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/attributes_setup.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/atom_helpers.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/atom_iterator.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/audio_processor.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/configure.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/dsp.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/from_atoms.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/helpers.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/init.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/inputs.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/message_processor.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/messages.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/outputs.hpp"
      "${AVND_SOURCE_DIR}/include/avnd/binding/max/to_atoms.hpp"
  )

  target_compile_definitions(
    ${AVND_FX_TARGET}
    PRIVATE
      AVND_MAXMSP=1
      $<$<BOOL:${WIN32}>:MAXAPI_USE_MSCRT=1>
      C74_USE_STRICT_TYPES=1
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
      maxmsp_commonsyms
  )

  if(APPLE)
    target_compile_definitions(${AVND_FX_TARGET} PUBLIC MAC_VERSION)
    set_property(TARGET ${AVND_FX_TARGET} PROPERTY BUNDLE True)
    set_property(TARGET ${AVND_FX_TARGET} PROPERTY BUNDLE_EXTENSION "mxo")
    target_link_libraries(${AVND_FX_TARGET} PUBLIC ${MAXSDK_LINKER_FLAGS} -Wl,-U,_class_dspinit -Wl,-U,_dsp_add64  -Wl,-U,_z_dsp_setup)
    file(COPY "${AVND_SOURCE_DIR}/include/avnd/binding/max/resources/PkgInfo" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/max/${AVND_C_NAME}.mxo/Contents/")

    # We only export ext_main to prevent conflicts in e.g. Max4Live.
    target_link_libraries(${AVND_FX_TARGET} PRIVATE "-Wl,-exported_symbols_list,'${CMAKE_CURRENT_BINARY_DIR}/maxmsp-symbols.txt'")
  elseif(WIN32)
    target_compile_definitions(${AVND_FX_TARGET} PUBLIC WIN_VERSION _CRT_SECURE_NO_WARNINGS)
    if("${CMAKE_SIZEOF_VOID_P}" MATCHES "8")
        set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".mxe64")
        find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}/x64")
        find_library(MAXSDK_MSP_LIBRARY NAMES MaxAudio.lib HINTS "${MAXSDK_MSP_INCLUDE_DIR}/x64")
    else()
        set_target_properties(${AVND_FX_TARGET} PROPERTIES SUFFIX ".mxe")
        find_library(MAXSDK_API_LIBRARY NAMES MaxAPI.lib HINTS "${MAXSDK_MAX_INCLUDE_DIR}")
        find_library(MAXSDK_MSP_LIBRARY NAMES MaxAudio.lib HINTS "${MAXSDK_MSP_INCLUDE_DIR}")
    endif()
    target_link_libraries(${AVND_FX_TARGET} PRIVATE ${MAXSDK_API_LIBRARY} ${MAXSDK_MSP_LIBRARY})
    # FIXME only export ext_main here too
  endif()

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_max INTERFACE)
target_link_libraries(Avendish_max INTERFACE Avendish)
add_library(Avendish::Avendish_max ALIAS Avendish_max)


### Utilities

function(avnd_create_max_package)
  cmake_parse_arguments(AVND
      "CODESIGN;NOTARIZE;INSTALL"
      "NAME;SOURCE_PATH;PACKAGE_ROOT;KEYCHAIN_FILE;KEYCHAIN_PASSWORD;CODESIGN_ENTITLEMENTS;CODESIGN_IDENTITY;NOTARIZE_TEAM;NOTARIZE_EMAIL;NOTARIZE_PASSWORD"
      "EXTERNALS;SUPPORT"
      ${ARGN})

  if(APPLE)
    if(NOT AVND_KEYCHAIN_FILE OR NOT AVND_KEYCHAIN_PASSWORD OR NOT AVND_CODESIGN_ENTITLEMENTS OR NOT AVND_CODESIGN_IDENTITY)
      message(STATUS "Disabling codesigning as the required avnd_create_max_package arguments are not set.")
      set(AVND_CODESIGN 0)
    endif()
    if(NOT AVND_NOTARIZE_TEAM OR NOT AVND_NOTARIZE_EMAIL OR NOT AVND_NOTARIZE_PASSWORD)
      message(STATUS "Disabling notarization as the required avnd_create_max_package arguments are not set.")
      set(AVND_NOTARIZE 0)
    endif()
  endif()

  set(_pkg_target "${AVND_NAME}_max_package")
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
    if(WIN32)
      add_custom_command(TARGET ${_external} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_pkg}/externals/"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_external}>" "${_pkg}/externals/"
      )
    else()
      set_target_properties(${_external} PROPERTIES
          BUILD_WITH_INSTALL_RPATH 1
          INSTALL_NAME_DIR "@rpath"
          BUILD_RPATH "@loader_path/../../../../support"
          INSTALL_RPATH "@loader_path/../../../../support"
      )

      if(AVND_CODESIGN)
        add_custom_command(TARGET ${_external} POST_BUILD
          COMMAND echo "=== codesign ==="
          COMMAND security unlock-keychain -p "${AVND_KEYCHAIN_PASSWORD}" "${AVND_KEYCHAIN_FILE}"
          COMMAND xcrun codesign --entitlements "${AVND_CODESIGN_ENTITLEMENTS}" --force --timestamp "--options=runtime" --sign "${AVND_CODESIGN_IDENTITY}" "$<TARGET_BUNDLE_DIR:${_external}>"
        )

        if(AVND_NOTARIZE)
          add_custom_command(TARGET ${_external} POST_BUILD
            COMMAND echo "=== notarize ==="
            COMMAND ${CMAKE_COMMAND} -E tar cf "$<TARGET_BUNDLE_DIR:${_external}>.zip" "--format=zip" "$<TARGET_BUNDLE_DIR:${_external}>"
            COMMAND xcrun notarytool submit "$<TARGET_BUNDLE_DIR:${_external}>.zip" --team-id "${AVND_NOTARIZE_TEAM}" --apple-id "${AVND_NOTARIZE_EMAIL}" --password "${AVND_NOTARIZE_PASSWORD}" --progress --wait

            COMMAND echo "=== staple ==="
            COMMAND xcrun stapler staple "$<TARGET_BUNDLE_DIR:${_external}>"
          )
        endif()
      endif()

      add_custom_command(TARGET ${_external} POST_BUILD
          COMMAND echo "=== copying $<TARGET_BUNDLE_DIR_NAME:${_external}> to ${_pkg}/externals/ ==="
          COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${_external}>" "${_pkg}/externals/$<TARGET_BUNDLE_DIR_NAME:${_external}>"
      )
    endif()
  endforeach()

  # Copy the support files
  foreach(_support ${AVND_SUPPORT})
    add_custom_command(TARGET ${_pkg_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_pkg}/support/"
      COMMAND ${CMAKE_COMMAND} -E copy "${_support}" "${_pkg}/support/"
    )
  endforeach()
endfunction()
