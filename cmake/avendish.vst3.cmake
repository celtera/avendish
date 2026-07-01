# Skip this back-end's setup (and its dependencies) entirely when it is disabled,
# so a single-back-end build -- e.g. one avnd-addon CI lane that passes
# -DAVND_ENABLE_VST3=OFF -- doesn't fetch/build what it won't use (godot-cpp, the
# VST3 SDK, ...). Matches _avnd_dispatch_backend: only act when explicitly OFF.
if(DEFINED AVND_ENABLE_VST3 AND NOT AVND_ENABLE_VST3)
  return()
endif()

# Build a self-contained VSTGUI editor into VST3 plug-ins (no Qt). Needs the
# VST3 SDK configured with SMTG_ADD_VSTGUI=ON so the `vstgui` target exists.
option(AVND_ENABLE_VST3_VSTGUI "Embed a VSTGUI editor in VST3 plug-ins" ON)

# VSTGUI on macOS compiles Objective-C++ sources, so the enclosing project must
# enable those languages (a plain `project(Foo CXX)` does not).
if(APPLE AND AVND_ENABLE_VST3_VSTGUI)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

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

# When the surrounding build uses the static CRT (/MT) -- e.g. an addon that ships
# a Max external forces CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded for the whole tree
# (see avendish.cmake) -- the VST3 SDK must use it too: it doesn't honour
# CMAKE_MSVC_RUNTIME_LIBRARY, so its base.lib stays /MD and clashes with the /MT
# objects at link time (LNK2038). Detect the static runtime directly (the "...DLL"
# variants are the dynamic /MD ones) rather than keying on AVND_ENABLE_MAX, so a
# single-backend build (e.g. the vst3-only CI lane, where MAX is off but the addon
# still set /MT globally) is covered too.
if(MSVC AND (AVND_ENABLE_MAX
    OR (CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded"
        AND NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")))
  set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
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
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_vst3.cpp"
  )

  # Compute the per-OS arch subdir per the VST3 spec layout.
  # https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Locations+Format/Plugin+Format.html
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _avnd_vst3_proc)
  if(APPLE)
    set(_avnd_vst3_archdir "MacOS")
  elseif(WIN32)
    if(_avnd_vst3_proc MATCHES "amd64|x86_64|x64")
      set(_avnd_vst3_archdir "x86_64-win")
    elseif(_avnd_vst3_proc MATCHES "arm64|aarch64")
      set(_avnd_vst3_archdir "arm64-win")
    elseif(_avnd_vst3_proc MATCHES "arm")
      set(_avnd_vst3_archdir "arm-win")
    else()
      set(_avnd_vst3_archdir "x86-win")
    endif()
  else()
    if(_avnd_vst3_proc MATCHES "amd64|x86_64|x64")
      set(_avnd_vst3_archdir "x86_64-linux")
    elseif(_avnd_vst3_proc MATCHES "aarch64|arm64")
      set(_avnd_vst3_archdir "aarch64-linux")
    elseif(_avnd_vst3_proc MATCHES "armv7|arm")
      set(_avnd_vst3_archdir "armv7l-linux")
    elseif(_avnd_vst3_proc MATCHES "i.86")
      set(_avnd_vst3_archdir "i386-linux")
    else()
      set(_avnd_vst3_archdir "${_avnd_vst3_proc}-linux")
    endif()
  endif()

  set(_avnd_vst3_outdir "vst3/${AVND_C_NAME}.vst3/Contents/${_avnd_vst3_archdir}")

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      LIBRARY_OUTPUT_DIRECTORY "${_avnd_vst3_outdir}"
      RUNTIME_OUTPUT_DIRECTORY "${_avnd_vst3_outdir}"
      ARCHIVE_OUTPUT_DIRECTORY "${_avnd_vst3_outdir}"
  )

  # VST3 spec wants <name>.so on Linux (no lib prefix), <name>.vst3 on Windows,
  # and a plain <name> binary inside MacOS/ on macOS.
  if(APPLE)
    set_target_properties(${AVND_FX_TARGET} PROPERTIES PREFIX "" SUFFIX "")
  elseif(WIN32)
    set_target_properties(${AVND_FX_TARGET} PROPERTIES PREFIX "" SUFFIX ".vst3")
  else()
    set_target_properties(${AVND_FX_TARGET} PROPERTIES PREFIX "" SUFFIX ".so")
  endif()

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

  # Optional self-contained VSTGUI editor. Requires the VST3 SDK to have been
  # configured with SMTG_ADD_VSTGUI=ON (which provides the `vstgui` target).
  if(AVND_ENABLE_VST3_VSTGUI AND TARGET vstgui)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE vstgui)
    target_compile_definitions(${AVND_FX_TARGET} PRIVATE AVND_VST3_VSTGUI=1)
    # VSTGUI headers are included as <vstgui/...>; the include root is the
    # vstgui4 dir (the `vstgui` target does not export it).
    if(SMTG_VSTGUI_SOURCE_DIR)
      target_include_directories(${AVND_FX_TARGET} PRIVATE "${SMTG_VSTGUI_SOURCE_DIR}")
    else()
      target_include_directories(${AVND_FX_TARGET} PRIVATE "${VST3_SDK_ROOT}/vstgui4")
    endif()

    # VSTGUI compiles its own sources with -Werror on macOS, which trips on
    # recent AppleClang/macOS SDKs (e.g. unused-variable in cgbitmap.cpp).
    # Don't let VSTGUI's internal warnings fail the addon build.
    if(APPLE)
      get_target_property(_avnd_vstgui_opts vstgui COMPILE_OPTIONS)
      if(_avnd_vstgui_opts)
        list(REMOVE_ITEM _avnd_vstgui_opts "-Werror")
        set_target_properties(vstgui PROPERTIES COMPILE_OPTIONS "${_avnd_vstgui_opts}")
      endif()
    endif()
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
