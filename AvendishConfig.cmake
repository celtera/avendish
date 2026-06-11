# find_package(Avendish) entry point, in order:
#  - a host already provides the avnd_* commands (e.g. score in-tree) -> use them.
#  - SCORE_SOURCE_DIR / SCORE_SDK set -> route through ScoreExternalAddon (the normal
#    external ossia score addon pathway: it sets up score and its SDK dependencies).
#  - otherwise -> build Avendish from these sources.
if(NOT COMMAND avnd_make_object AND NOT COMMAND avnd_score_plugin_add)
  if(DEFINED SCORE_SOURCE_DIR AND EXISTS "${SCORE_SOURCE_DIR}/cmake/ScoreExternalAddon.cmake")
    include("${SCORE_SOURCE_DIR}/cmake/ScoreExternalAddon.cmake")
  elseif(DEFINED SCORE_SDK AND EXISTS "${SCORE_SDK}/lib/cmake/score/ScoreExternalAddon.cmake")
    include("${SCORE_SDK}/lib/cmake/score/ScoreExternalAddon.cmake")
  endif()
endif()

if(NOT COMMAND avnd_make_object AND NOT COMMAND avnd_score_plugin_add)
  include(CMakeFindDependencyMacro)
  find_dependency(Boost)
  find_dependency(Threads)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}" avnd_build)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/AvendishAddon.cmake")

set(Avendish_FOUND TRUE)
