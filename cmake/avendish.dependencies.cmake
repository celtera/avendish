include(FetchContent)
FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY "https://github.com/jcelerier/concurrentqueue"
  GIT_TAG master
  GIT_PROGRESS true
)
FetchContent_MakeAvailable(concurrentqueue)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY "https://github.com/nlohmann/json"
  GIT_TAG master
  GIT_PROGRESS true
)
FetchContent_MakeAvailable(nlohmann_json)

set(BUILD_TESTING 0)
set(BUILD_BENCHMARK 0)
set(INJA_USE_EMBEDDED_JSON 0)
FetchContent_Declare(
  pantor_inja
  GIT_REPOSITORY "https://github.com/pantor/inja"
  GIT_TAG master
  GIT_PROGRESS true
)
FetchContent_MakeAvailable(pantor_inja)
