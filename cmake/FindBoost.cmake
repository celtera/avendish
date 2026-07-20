# Minimal FindBoost for CMake >= 4 (which removed the bundled module).
#
# Avendish only needs Boost headers (Boost::boost / Boost::headers). Strategy:
#  1. defer to Boost's own BoostConfig.cmake when it is installed;
#  2. otherwise, satisfy the request from a plain header tree given via
#     BOOST_ROOT / Boost_INCLUDE_DIR.

# 1. Prefer config mode if available.
find_package(Boost ${Boost_FIND_VERSION} CONFIG QUIET)
if(Boost_FOUND)
  return()
endif()

# 2. Header-only fallback.
if(NOT Boost_INCLUDE_DIR)
  if(BOOST_ROOT)
    set(Boost_INCLUDE_DIR "${BOOST_ROOT}")
  elseif(DEFINED ENV{BOOST_ROOT})
    set(Boost_INCLUDE_DIR "$ENV{BOOST_ROOT}")
  endif()
endif()

if(Boost_INCLUDE_DIR AND EXISTS "${Boost_INCLUDE_DIR}/boost/version.hpp")
  file(STRINGS "${Boost_INCLUDE_DIR}/boost/version.hpp" _boost_ver_line
       REGEX "#define BOOST_LIB_VERSION ")
  string(REGEX MATCH "\"([0-9]+)_([0-9]+)\"" _ "${_boost_ver_line}")
  set(Boost_VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(Boost_VERSION_MINOR "${CMAKE_MATCH_2}")
  set(Boost_VERSION_STRING "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.0")
  set(Boost_VERSION "${Boost_VERSION_STRING}")
  set(Boost_FOUND TRUE)

  if(NOT TARGET Boost::boost)
    add_library(Boost::boost INTERFACE IMPORTED GLOBAL)
    set_target_properties(Boost::boost PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIR}")
  endif()
  if(NOT TARGET Boost::headers)
    add_library(Boost::headers INTERFACE IMPORTED GLOBAL)
    set_target_properties(Boost::headers PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIR}")
  endif()
else()
  set(Boost_FOUND FALSE)
  if(Boost_FIND_REQUIRED)
    message(FATAL_ERROR
      "FindBoost shim: Boost not found. Install Boost with CMake config "
      "files, or pass -DBOOST_ROOT=<path to a Boost header tree>.")
  endif()
endif()
