# https://github.com/godotengine/godot-cpp/pull/1961 fixes the same missing
# allocation declarations, but has not been backported to the 4.5 branch.
# Use the C header to declare the existing unqualified realloc/free calls.
set(_source "${GODOT_CPP_SOURCE_DIR}/src/godot.cpp")
file(READ "${_source}" _content)
if(_content MATCHES "#include[ \t]*[<\"](cstdlib|stdlib\\.h)[>\"]")
  return()
endif()

set(_anchor "#include <stdio.h>")
string(FIND "${_content}" "${_anchor}" _position)
if(_position EQUAL -1)
  message(FATAL_ERROR "Cannot apply godot-cpp allocation-header patch: ${_source} has changed")
endif()
string(REPLACE "${_anchor}" "${_anchor}\n#include <stdlib.h>" _content "${_content}")
file(WRITE "${_source}" "${_content}")
