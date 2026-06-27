# Best-effort file copy used by avendish.packaging POST_BUILD steps: copy SRC into
# the DST directory only if SRC exists, never failing the build. Used for inputs
# that may legitimately be absent on some toolchains (e.g. a .maxref.xml that the
# maxref generator doesn't emit on every platform).
#   cmake -DSRC=<file> -DDST=<dir> -P avendish.packaging.copy_optional.cmake
if(EXISTS "${SRC}")
  file(MAKE_DIRECTORY "${DST}")
  file(COPY "${SRC}" DESTINATION "${DST}")
else()
  message(STATUS "avendish.packaging: optional file not present, skipping: ${SRC}")
endif()
