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

if(NOT TARGET Avendish_soft_ui)
  add_library(Avendish_soft_ui INTERFACE)
  target_link_libraries(Avendish_soft_ui INTERFACE avnd_deps_nuklear avnd_deps_canvas_ity)
  add_library(Avendish::soft_ui ALIAS Avendish_soft_ui)

  # Default font for examples & tests (Nuklear vendors a few permissively
  # licensed TTFs in extra_font/).
  set(AVND_SOFT_UI_DEFAULT_FONT "${avnd_nuklear_SOURCE_DIR}/extra_font/ProggyClean.ttf"
      CACHE FILEPATH "Default TTF for the soft UI backend")
endif()

function(avnd_target_soft_ui theTarget)
  target_link_libraries("${theTarget}" PRIVATE Avendish::soft_ui)
  if(EXISTS "${AVND_SOFT_UI_DEFAULT_FONT}")
    target_compile_definitions("${theTarget}" PRIVATE
      "AVND_SOFT_UI_DEFAULT_FONT=\"${AVND_SOFT_UI_DEFAULT_FONT}\"")
  endif()
endfunction()

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/framebuffer.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/fonts.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/implementation.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/nk_config.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/nk_renderer.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/painter.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/runtime.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/ui/soft/surface.hpp"
)
