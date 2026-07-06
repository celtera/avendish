# Software UI backend (CUSTOM_UI_PLAN.md): Nuklear (standard widgets,
# command-list output) + canvas_ity (software vector rasterizer implementing
# avnd::painter). Both are tiny permissive single-header libraries with zero
# dependencies, fetched here and exposed through the Avendish::soft_ui
# interface target.
#
# Skip entirely when disabled so single-back-end builds fetch nothing.
if(DEFINED AVND_ENABLE_SOFT_UI AND NOT AVND_ENABLE_SOFT_UI)
  function(avnd_target_soft_ui)
  endfunction()
  return()
endif()

# The desktop shell needs a windowing system: without the X11 development
# headers on Linux there is nothing to embed editors in, so skip the whole
# backend gracefully (plug-ins build UI-less) instead of failing to
# configure.
if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  find_package(X11 QUIET)
  if(NOT X11_FOUND)
    message(STATUS "avendish: X11 development headers not found, soft UI editors disabled")
    function(avnd_target_soft_ui)
    endfunction()
    return()
  endif()
endif()

include(FetchContent)

if(NOT TARGET avnd_deps_nuklear)
  FetchContent_Declare(
    avnd_nuklear
    GIT_REPOSITORY "https://github.com/Immediate-Mode-UI/Nuklear"
    GIT_TAG master
    GIT_PROGRESS true
  )
  FetchContent_Declare(
    avnd_canvas_ity
    GIT_REPOSITORY "https://github.com/a-e-k/canvas_ity"
    GIT_TAG main
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(avnd_nuklear avnd_canvas_ity)

  add_library(avnd_deps_nuklear INTERFACE)
  target_include_directories(avnd_deps_nuklear INTERFACE "${avnd_nuklear_SOURCE_DIR}")

  add_library(avnd_deps_canvas_ity INTERFACE)
  target_include_directories(avnd_deps_canvas_ity INTERFACE "${avnd_canvas_ity_SOURCE_DIR}/src")
endif()

# pugl: tiny windowing shim made for plug-in editor embedding
# (puglSetParent + host-pumped events). Desktop only; wasm and MCU shells
# talk to soft_ui::surface directly.
if(NOT EMSCRIPTEN AND NOT TARGET avnd_pugl)
  FetchContent_Declare(
    avnd_pugl
    GIT_REPOSITORY "https://github.com/lv2/pugl"
    GIT_TAG b7637149ebe53124e5be90559e02a0185bbcbd73
    GIT_PROGRESS true
  )
  FetchContent_GetProperties(avnd_pugl)
  if(NOT avnd_pugl_POPULATED)
    FetchContent_Populate(avnd_pugl)
  endif()

  set(_pugl_src
    "${avnd_pugl_SOURCE_DIR}/src/common.c"
    "${avnd_pugl_SOURCE_DIR}/src/internal.c"
  )
  if(WIN32)
    list(APPEND _pugl_src
      "${avnd_pugl_SOURCE_DIR}/src/win.c"
      "${avnd_pugl_SOURCE_DIR}/src/win_stub.c")
  elseif(APPLE)
    list(APPEND _pugl_src
      "${avnd_pugl_SOURCE_DIR}/src/mac.m"
      "${avnd_pugl_SOURCE_DIR}/src/mac_stub.m")
  else()
    list(APPEND _pugl_src
      "${avnd_pugl_SOURCE_DIR}/src/x11.c"
      "${avnd_pugl_SOURCE_DIR}/src/x11_stub.c")
  endif()

  add_library(avnd_pugl STATIC ${_pugl_src})
  target_include_directories(avnd_pugl PUBLIC "${avnd_pugl_SOURCE_DIR}/include")
  target_compile_definitions(avnd_pugl PUBLIC PUGL_STATIC)
  set_target_properties(avnd_pugl PROPERTIES POSITION_INDEPENDENT_CODE ON)

  if(WIN32)
    target_link_libraries(avnd_pugl PUBLIC user32 gdi32 dwmapi shlwapi shell32)
  elseif(APPLE)
    # QuartzCore/CoreGraphics: the soft window's CALayer blit
    target_link_libraries(avnd_pugl PUBLIC
      "-framework Cocoa" "-framework CoreVideo"
      "-framework QuartzCore" "-framework CoreGraphics")
  else()
    find_package(X11 REQUIRED) # probed QUIET above; diagnose here if racing configs
    target_link_libraries(avnd_pugl PUBLIC X11::X11)
    if(TARGET X11::Xrandr)
      target_link_libraries(avnd_pugl PUBLIC X11::Xrandr)
      target_compile_definitions(avnd_pugl PRIVATE USE_XRANDR=1)
    endif()
    if(TARGET X11::Xcursor)
      target_link_libraries(avnd_pugl PUBLIC X11::Xcursor)
      target_compile_definitions(avnd_pugl PRIVATE USE_XCURSOR=1)
    endif()
  endif()
endif()

if(NOT TARGET Avendish_soft_ui)
  add_library(Avendish_soft_ui INTERFACE)
  target_link_libraries(Avendish_soft_ui INTERFACE avnd_deps_nuklear avnd_deps_canvas_ity)
  if(TARGET avnd_pugl)
    target_link_libraries(Avendish_soft_ui INTERFACE avnd_pugl)
  endif()
  add_library(Avendish::soft_ui ALIAS Avendish_soft_ui)

  # Default font for examples & tests (Nuklear vendors a few permissively
  # licensed TTFs in extra_font/).
  set(AVND_SOFT_UI_DEFAULT_FONT "${avnd_nuklear_SOURCE_DIR}/extra_font/ProggyClean.ttf"
      CACHE FILEPATH "Default TTF for the soft UI backend")
endif()

function(avnd_target_soft_ui theTarget)
  target_link_libraries("${theTarget}" PRIVATE Avendish::soft_ui)
  # Editor-capable targets (plug-in bindings) resolve declarative `struct ui`
  # to the soft editor through avnd/binding/ui/editor.hpp.
  if(TARGET avnd_pugl)
    target_compile_definitions("${theTarget}" PRIVATE AVND_SOFT_UI_EDITOR=1)
  endif()
  if(EXISTS "${AVND_SOFT_UI_DEFAULT_FONT}")
    if(NOT EMSCRIPTEN)
      target_compile_definitions("${theTarget}" PRIVATE
        "AVND_SOFT_UI_DEFAULT_FONT=\"${AVND_SOFT_UI_DEFAULT_FONT}\"")
    else()
      # No --embed-file here: the FS emulation it drags in aborts inside
      # AudioWorkletGlobalScope, and the same module runs in the worklet.
      # The page fetches font.ttf and hands it to SoftUI.loadFont instead;
      # ship the file next to the module.
      add_custom_command(TARGET "${theTarget}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${AVND_SOFT_UI_DEFAULT_FONT}" "$<TARGET_FILE_DIR:${theTarget}>/font.ttf"
      )
    endif()
  endif()
endfunction()

# Standalone UI preview executables: run any example's editor in a plain
# window, no plug-in host needed. `<exe> --frames N` exits after N cycles.
function(avnd_make_ui_preview)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})
  if(NOT TARGET avnd_pugl)
    return()
  endif()

  set(AVND_FX_TARGET "${AVND_TARGET}_ui_preview")
  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/preview_prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_ui_preview.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )
  add_executable(${AVND_FX_TARGET}
    "${AVND_MAIN_FILE}"
    "${CMAKE_BINARY_DIR}/${AVND_C_NAME}_ui_preview.cpp"
  )
  set_target_properties(${AVND_FX_TARGET} PROPERTIES
    OUTPUT_NAME "${AVND_C_NAME}_ui"
    RUNTIME_OUTPUT_DIRECTORY ui_preview
  )
  avnd_target_soft_ui(${AVND_FX_TARGET})
  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")
endfunction()

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/framebuffer.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/fonts.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/implementation.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/nk_config.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/nk_renderer.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/painter.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/runtime.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/standalone.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/surface.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/theme.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/wasm.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/window.hpp"
)
