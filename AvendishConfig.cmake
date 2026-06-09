# find_package(Avendish) entry point. Works both as a fresh dependency and when a host
# (e.g. ossia score) already provides Avendish in its own tree: in the latter case the
# avnd_* commands already exist, so we must not re-add the sources or re-find deps.
if(NOT COMMAND avnd_make_object AND NOT COMMAND avnd_score_plugin_add)
  include(CMakeFindDependencyMacro)
  find_dependency(Boost)
  find_dependency(Threads)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}" avnd_build)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AvendishAddon.cmake")

set(Avendish_FOUND TRUE)
