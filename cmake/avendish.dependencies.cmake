include(FetchContent)

if(NOT TARGET fmt::fmt)
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt"
    GIT_TAG 11.0.1
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(fmt)
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

if(NOT TARGET magic_enum)
  FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY "https://github.com/Neargye/magic_enum"
    GIT_TAG master
    GIT_PROGRESS true
  )
  FetchContent_MakeAvailable(magic_enum)
  include_directories("${magic_enum_SOURCE_DIR}/include")
endif()
