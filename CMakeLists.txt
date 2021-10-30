cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(avendish)

include(avendish)

# Only build examples and tests if we are building this repo directly,
# not if it is add_subdirectory'd
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  include(avendish.examples)
  include(avendish.tests)
endif()
