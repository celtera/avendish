cmake_minimum_required(VERSION 4.1 FATAL_ERROR)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(avendish LANGUAGES C CXX)
option(AVENDISH_INCLUDE_SOURCE_ONLY "Create a simple Avendish target for including headers" OFF)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

if(AVENDISH_INCLUDE_SOURCE_ONLY)
  set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")
  include(cmake/avendish.sources.cmake)
  return()
endif()

include(cmake/avendish.cmake)

# Only build examples and tests if we are building this repo directly,
# not if it is add_subdirectory'd
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  include(avendish.examples)
  include(avendish.tests)
endif()
