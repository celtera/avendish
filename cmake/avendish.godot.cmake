# Skip this back-end's setup (and its dependencies) entirely when it is disabled,
# so a single-back-end build -- e.g. one avnd-addon CI lane that passes
# -DAVND_ENABLE_GODOT=OFF -- doesn't fetch/build what it won't use (godot-cpp, the
# VST3 SDK, ...). Matches _avnd_dispatch_backend: only act when explicitly OFF.
if(DEFINED AVND_ENABLE_GODOT AND NOT AVND_ENABLE_GODOT)
  # examples.cmake calls avnd_make_godot directly; keep the command defined.
  function(avnd_make_godot)
  endfunction()
  return()
endif()

# Godot GDExtension binding for Avendish
#
# Requires godot-cpp (https://github.com/godotengine/godot-cpp).
# Set GODOT_CPP_PATH to the path of the godot-cpp source directory,
# or ensure godot-cpp is available via find_package.
#
# Usage in .gdextension file:
#   [configuration]
#   entry_symbol = "avnd_<c_name>_init"
#
#   [libraries]
#   linux.debug.x86_64 = "res://bin/lib<c_name>.so"
#   windows.debug.x86_64 = "res://bin/<c_name>.dll"

# Try to find godot-cpp
set(_GODOT_CPP_FOUND FALSE)
set(GODOTCPP_DEBUG_CRT "$<CONFIG:Debug>")

# Godot executable used for the headless GDExtension smoke tests.
# Not required for building; the tests are skipped when it is absent.
find_program(AVND_GODOT_EXECUTABLE NAMES godot godot4 Godot)
if(GODOT_CPP_PATH)
  if(IS_DIRECTORY "${GODOT_CPP_PATH}" AND NOT TARGET godot-cpp)
    add_subdirectory("${GODOT_CPP_PATH}" godot-cpp EXCLUDE_FROM_ALL)
  endif()
  if(TARGET godot-cpp)
    set(_GODOT_CPP_FOUND TRUE)
  endif()
else()
  include(FetchContent)
  FetchContent_Declare(
      godot-cpp
      GIT_REPOSITORY  https://github.com/godotengine/godot-cpp.git
      GIT_TAG         4.5
      GIT_SHALLOW     TRUE
  )
  FetchContent_MakeAvailable(godot-cpp)
endif()

if(NOT _GODOT_CPP_FOUND)
  find_package(godot-cpp QUIET CONFIG)
  if(TARGET godot-cpp::godot-cpp)
    set(_GODOT_CPP_FOUND TRUE)
  elseif(TARGET godot-cpp)
    set(_GODOT_CPP_FOUND TRUE)
  endif()
endif()

if(NOT _GODOT_CPP_FOUND)
  function(avnd_make_godot)
  endfunction()
  return()
endif()

# Determine the correct target name
if(TARGET godot-cpp::godot-cpp)
  set(AVND_GODOT_CPP_TARGET godot-cpp::godot-cpp)
elseif(TARGET godot-cpp)
  set(AVND_GODOT_CPP_TARGET godot-cpp)
endif()

set(AVND_GODOT_SOURCES
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/all.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/node.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/audio_effect.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/texture_node.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/buffer_node.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/geometry_node.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/prototype.cpp.in"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/audio_effect.prototype.cpp.in"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/texture_node.prototype.cpp.in"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/buffer_node.prototype.cpp.in"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/geometry_node.prototype.cpp.in"
  "${AVND_SOURCE_DIR}/include/avnd/binding/godot/gdextension.in"

  CACHE "" INTERNAL
)

# PROCESSOR_TYPE: NODE (default), AUDIO_EFFECT, TEXTURE, BUFFER, or GEOMETRY
function(avnd_make_godot)
  cmake_parse_arguments(AVND "" "PROCESSOR_TYPE;TARGET;MAIN_FILE;MAIN_CLASS;C_NAME;EXAMPLE_GODOT" "" ${ARGN})

  if(NOT AVND_PROCESSOR_TYPE)
    set(AVND_PROCESSOR_TYPE "NODE")
  endif()

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}_${AVND_PROCESSOR_TYPE}" MAIN_OUT_FILE)

  if(AVND_PROCESSOR_TYPE STREQUAL "AUDIO_EFFECT")
    set(_AVND_GODOT_PROTOTYPE "${AVND_SOURCE_DIR}/include/avnd/binding/godot/audio_effect.prototype.cpp.in")
    set(_AVND_GODOT_SUFFIX "_fx")
  elseif(AVND_PROCESSOR_TYPE STREQUAL "TEXTURE")
    set(_AVND_GODOT_PROTOTYPE "${AVND_SOURCE_DIR}/include/avnd/binding/godot/texture_node.prototype.cpp.in")
    set(_AVND_GODOT_SUFFIX "_tex")
  elseif(AVND_PROCESSOR_TYPE STREQUAL "BUFFER")
    set(_AVND_GODOT_PROTOTYPE "${AVND_SOURCE_DIR}/include/avnd/binding/godot/buffer_node.prototype.cpp.in")
    set(_AVND_GODOT_SUFFIX "_buf")
  elseif(AVND_PROCESSOR_TYPE STREQUAL "GEOMETRY")
    set(_AVND_GODOT_PROTOTYPE "${AVND_SOURCE_DIR}/include/avnd/binding/godot/geometry_node.prototype.cpp.in")
    set(_AVND_GODOT_SUFFIX "_geo")
  else()
    set(_AVND_GODOT_PROTOTYPE "${AVND_SOURCE_DIR}/include/avnd/binding/godot/prototype.cpp.in")
    set(_AVND_GODOT_SUFFIX "")
  endif()

  configure_file(
    "${_AVND_GODOT_PROTOTYPE}"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_godot.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/godot/gdextension.in"
    "${CMAKE_BINARY_DIR}/godot/${AVND_C_NAME}${_AVND_GODOT_SUFFIX}.gdextension"
    @ONLY
    NEWLINE_STYLE LF
  )

  set(AVND_FX_TARGET "${AVND_TARGET}_${AVND_PROCESSOR_TYPE}_godot")
  add_library(${AVND_FX_TARGET} SHARED)

  # Register for avnd_addon_package() (the addon-wide packaging orchestrator):
  # the binary target + its generated .gdextension manifest.
  set_property(GLOBAL APPEND PROPERTY AVND_ALL_GODOT_EXTERNALS ${AVND_FX_TARGET})
  set_property(GLOBAL APPEND PROPERTY AVND_ALL_GODOT_GDEXTENSIONS
    "${CMAKE_BINARY_DIR}/godot/${AVND_C_NAME}${_AVND_GODOT_SUFFIX}.gdextension")

  if(APPLE)
    # macOS: emit a .framework bundle so the binary can be codesigned
    # with --deep --options runtime AND stapled via xcrun stapler staple
    # (loose .dylibs cannot be stapled).
    set(_avnd_godot_plist "${CMAKE_BINARY_DIR}/${AVND_C_NAME}${_AVND_GODOT_SUFFIX}_godot_Info.plist")
    configure_file(
      "${AVND_SOURCE_DIR}/include/avnd/binding/godot/Framework.Info.plist.in"
      "${_avnd_godot_plist}"
      @ONLY
    )
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "${AVND_C_NAME}${_AVND_GODOT_SUFFIX}"
        FRAMEWORK TRUE
        MACOSX_FRAMEWORK_BUNDLE_VERSION 1.0
        MACOSX_FRAMEWORK_SHORT_VERSION_STRING 1.0
        MACOSX_FRAMEWORK_IDENTIFIER "ossia.${AVND_C_NAME}${_AVND_GODOT_SUFFIX}"
        MACOSX_FRAMEWORK_BUNDLE_NAME "${AVND_C_NAME}${_AVND_GODOT_SUFFIX}"
        MACOSX_FRAMEWORK_INFO_PLIST "${_avnd_godot_plist}"
        LIBRARY_OUTPUT_DIRECTORY godot
        RUNTIME_OUTPUT_DIRECTORY godot
        ARCHIVE_OUTPUT_DIRECTORY godot
        CXX_STANDARD 23
        CXX_EXTENSIONS OFF
    )
  else()
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        OUTPUT_NAME "${AVND_C_NAME}${_AVND_GODOT_SUFFIX}"
        LIBRARY_OUTPUT_DIRECTORY godot
        RUNTIME_OUTPUT_DIRECTORY godot
        ARCHIVE_OUTPUT_DIRECTORY godot
        PREFIX ""
        CXX_STANDARD 23
        CXX_EXTENSIONS OFF
    )
  endif()

  if(TARGET ${AVND_TARGET})
    target_compile_features(
      ${AVND_TARGET}
      PUBLIC
        cxx_std_23
    )
  endif()
  target_compile_features(
    ${AVND_FX_TARGET}
    PUBLIC
      cxx_std_23
  )
  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_godot.cpp"
      ${AVND_GODOT_SOURCES}
  )

  # Resolve the godot-cpp target here: AVND_GODOT_CPP_TARGET is set at directory scope
  # and is invisible when this function is called from an addon's own scope, which would
  # drop godot-cpp's generated (gen/include) headers from the GDExtension's include path.
  if(TARGET godot-cpp::godot-cpp)
    set(_avnd_godot_cpp godot-cpp::godot-cpp)
  elseif(TARGET godot-cpp)
    set(_avnd_godot_cpp godot-cpp)
  else()
    set(_avnd_godot_cpp "${AVND_GODOT_CPP_TARGET}")
  endif()
  target_link_libraries(
    ${AVND_FX_TARGET}
    PRIVATE
      Avendish::Avendish_godot
      ${_avnd_godot_cpp}
  )

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  # Example scene: instantiates the registered class (avnd_<c_name><suffix>).
  avnd_generate_help(
    FX_TARGET     "${AVND_FX_TARGET}"
    SOURCE_TARGET "${AVND_TARGET}"
    BACKEND       godot
    DESTINATION   "godot/examples/$<IF:${multi_config},$<CONFIG>/,>${AVND_C_NAME}${_AVND_GODOT_SUFFIX}.tscn"
    PROPERTY      AVND_GODOT_EXAMPLE
    OVERRIDE      "${AVND_EXAMPLE_GODOT}"
    EXTRA_ARG     "avnd_${AVND_C_NAME}${_AVND_GODOT_SUFFIX}"
  )

  # Headless smoke test: load the extension in godot --headless and check
  # that the class registers. Runs for any project that has a godot
  # executable available (set AVND_GODOT_EXECUTABLE or put godot in PATH),
  # so plugins and templates built on avendish get it for free.
  if(BUILD_TESTING AND AVND_GODOT_EXECUTABLE)
    add_test(
      NAME ${AVND_FX_TARGET}_smoke
      COMMAND
        "${CMAKE_COMMAND}"
        -DGODOT=${AVND_GODOT_EXECUTABLE}
        -DLIBRARY=$<IF:$<BOOL:$<TARGET_PROPERTY:${AVND_FX_TARGET},FRAMEWORK>>,$<TARGET_BUNDLE_DIR:${AVND_FX_TARGET}>,$<TARGET_FILE:${AVND_FX_TARGET}>>
        -DGDEXTENSION=${CMAKE_BINARY_DIR}/godot/${AVND_C_NAME}${_AVND_GODOT_SUFFIX}.gdextension
        -DCLASS=avnd_${AVND_C_NAME}${_AVND_GODOT_SUFFIX}
        -DWORK_DIR=${CMAKE_BINARY_DIR}/godot-smoke/${AVND_FX_TARGET}
        -P "${AVND_SOURCE_DIR}/cmake/avendish.godot.smoketest.cmake"
    )
  endif()

  message(STATUS "Configured Godot GDExtension: ${AVND_FX_TARGET}")
endfunction()

add_library(Avendish_godot INTERFACE)
target_link_libraries(Avendish_godot INTERFACE Avendish)
add_library(Avendish::Avendish_godot ALIAS Avendish_godot)

target_sources(Avendish PRIVATE ${AVND_GODOT_SOURCES})
