include(CMakeFindDependencyMacro)

set(CMAKE_INCLUDE_CURRENT_DIR 1)
set(CMAKE_CXX_STANDARD_REQUIRED 20)
set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

find_package(Boost QUIET REQUIRED)
find_package(Threads QUIET)
find_package(fmt QUIET)

include(avendish.disableexceptions)
include(avendish.sources)

include(avendish.ui.qt)
include(avendish.max)
include(avendish.pd)
include(avendish.python)
include(avendish.vintage)
include(avendish.vst3)
include(avendish.clap)
include(avendish.ossia)
include(avendish.standalone)
include(avendish.example)

# Used for getting completion in IDEs...
function(avnd_register)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})
  target_sources(Avendish PRIVATE "${AVND_MAIN_FILE}")
endfunction()

# Bindings to programming languages, things with a proper object model
function(avnd_make_object)
  avnd_register(${ARGV})

  avnd_make_ossia(${ARGV})
  avnd_make_python(${ARGV})
  avnd_make_pd(${ARGV})
  avnd_make_max(${ARGV})
  avnd_make_standalone(${ARGV})
  avnd_make_example_host(${ARGV})
endfunction()

# Bindings to audio plug-in APIs
function(avnd_make_audioplug)
  avnd_register(${ARGV})

  avnd_make_ossia(${ARGV})
  avnd_make_vintage(${ARGV})
  avnd_make_clap(${ARGV})
  avnd_make_vst3(${ARGV})
  avnd_make_example_host(${ARGV})
endfunction()

function(avnd_make_all)
  avnd_register(${ARGV})
  avnd_make_ossia(${ARGV})
  avnd_make_example_host(${ARGV})
  avnd_make_object(${ARGV})
  avnd_make_audioplug(${ARGV})
endfunction()
