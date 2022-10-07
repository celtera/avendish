include(CMakeFindDependencyMacro)

find_dependency(Boost)
find_dependency(Threads)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}" avnd_build)
