include(CMakeFindDependencyMacro)

find_dependency(Python COMPONENTS Interpreter Development)
find_dependency(pybind11 CONFIG)
find_dependency(Boost QUIET REQUIRED)
find_dependency(Threads QUIET)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}" avnd_build)
