# Minimal FindBoost for CMake >= 4 (which removed the bundled module).
#
# Avendish only needs Boost headers (Boost::boost / Boost::headers). Strategy:
#  1. defer to Boost's own BoostConfig.cmake when it is installed;
#  2. otherwise, satisfy the request from a plain header tree, accepting both
#     the source layout (<root>/boost/version.hpp) and the installed one
#     (<root>/include/boost/version.hpp), and falling back to the default
#     search paths.

# 1. Prefer config mode if available.
find_package(Boost ${Boost_FIND_VERSION} CONFIG QUIET)
if(Boost_FOUND)
  return()
endif()

# 2. Header-only fallback.
if(NOT Boost_INCLUDE_DIR)
  set(_avnd_boost_hints "")
  foreach(_avnd_boost_root
          "${BOOST_ROOT}" "$ENV{BOOST_ROOT}"
          "${Boost_ROOT}" "$ENV{Boost_ROOT}"
          "${BOOSTROOT}" "$ENV{BOOSTROOT}")
    if(_avnd_boost_root)
      list(APPEND _avnd_boost_hints "${_avnd_boost_root}")
    endif()
  endforeach()

  # PATH_SUFFIXES covers installed trees (<root>/include/boost/version.hpp);
  # the bare hint covers unpacked source trees (<root>/boost/version.hpp).
  find_path(Boost_INCLUDE_DIR
    NAMES boost/version.hpp
    HINTS ${_avnd_boost_hints}
    PATH_SUFFIXES include)
  unset(_avnd_boost_hints)
  unset(_avnd_boost_root)
endif()

if(Boost_INCLUDE_DIR AND EXISTS "${Boost_INCLUDE_DIR}/boost/version.hpp")
  file(STRINGS "${Boost_INCLUDE_DIR}/boost/version.hpp" _avnd_boost_ver_line
       REGEX "#define BOOST_LIB_VERSION ")
  string(REGEX MATCH "\"([0-9]+)_([0-9]+)" _ "${_avnd_boost_ver_line}")
  set(Boost_VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(Boost_VERSION_MINOR "${CMAKE_MATCH_2}")
  set(Boost_VERSION_PATCH 0)
  set(Boost_VERSION_STRING "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.0")
  set(Boost_VERSION "${Boost_VERSION_STRING}")
  unset(_avnd_boost_ver_line)

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
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Boost
  REQUIRED_VARS Boost_INCLUDE_DIR
  VERSION_VAR Boost_VERSION_STRING
  FAIL_MESSAGE
    "Boost headers not found. Install Boost with CMake config files, or pass \
-DBOOST_ROOT=<path to a Boost tree>.")
