include(FetchContent)

if(NOT TARGET fmt::fmt AND NOT TARGET fmt::fmt_header_only)
  # fmt >= 12 auto-enables its C++-modules target when CMake supports it and
  # then hard-requires a module-scanning toolchain; we never want that here.
  set(FMT_MODULE OFF CACHE BOOL "Build fmt as a C++ module")
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt"
    GIT_TAG 12.2.0
    GIT_PROGRESS true
  )
  if(EMSCRIPTEN)
    # fmt's compiled lib fails against emscripten's libc++; expose header-only.
    FetchContent_GetProperties(fmt)
    if(NOT fmt_POPULATED)
      FetchContent_Populate(fmt)
    endif()
    add_library(fmt_header_only INTERFACE)
    target_include_directories(fmt_header_only INTERFACE "${fmt_SOURCE_DIR}/include")
    target_compile_definitions(fmt_header_only INTERFACE FMT_HEADER_ONLY=1)
    add_library(fmt::fmt_header_only ALIAS fmt_header_only)
  else()
    FetchContent_MakeAvailable(fmt)
  endif()
endif()

if(NOT TARGET concurrentqueue)
  FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY "https://github.com/jcelerier/concurrentqueue"
    GIT_TAG master
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(concurrentqueue)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY "https://github.com/nlohmann/json"
    GIT_TAG master
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# yyjson: lightweight C JSON library used by the dump backend's writer.
if(NOT TARGET yyjson)
  block()
  set(YYJSON_BUILD_TESTS 0)
  set(YYJSON_BUILD_MISC 0)
  FetchContent_Declare(
    yyjson
    GIT_REPOSITORY "https://github.com/ibireme/yyjson"
    GIT_TAG 0.12.0
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(yyjson)
  endblock()
endif()

if(NOT TARGET pantor::inja)
  block()
  set(BUILD_TESTING 0)
  set(BUILD_BENCHMARK 0)
  set(INJA_USE_EMBEDDED_JSON 0)
  FetchContent_Declare(
    pantor_inja
    GIT_REPOSITORY "https://github.com/pantor/inja"
    GIT_TAG main
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(pantor_inja)
  endblock()
endif()

if(NOT TARGET qlibs::reflect)
  FetchContent_Declare(
    qlibs_reflect
    GIT_REPOSITORY "https://github.com/qlibs/reflect"
    GIT_TAG main
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(qlibs_reflect)
  add_library(qlibs_reflect INTERFACE)
  add_library(qlibs::reflect ALIAS qlibs_reflect)
  target_include_directories(qlibs_reflect INTERFACE "${qlibs_reflect_SOURCE_DIR}")
  include_directories("${qlibs_reflect_SOURCE_DIR}")
endif()

if(NOT TARGET magic_enum::magic_enum)
  FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY "https://github.com/Neargye/magic_enum"
    GIT_TAG master
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(magic_enum)
endif()
