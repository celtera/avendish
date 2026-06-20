# C++20 module support.
#
# A user object can be written as a module (`export module <c_name>; import halp;`)
# in a .cppm MAIN_FILE instead of a header. The same per-backend prototype then
# either `#include`s the header or `import`s the module -- only that one line
# differs -- and a .cppm is added to the backend target as a C++20 module source
# linking the shared halp module. Module-ness is auto-detected from the .cppm
# extension; the module name is the object's C_NAME by convention.

# Line a prototype uses to pull in the user object.
function(avnd_main_import out main_file c_name)
  if("${main_file}" MATCHES "\\.cppm$")
    set("${out}" "import ${c_name};" PARENT_SCOPE)
  else()
    set("${out}" "#include <${main_file}>" PARENT_SCOPE)
  endif()
endfunction()

# Add the user object to its base library. A .cppm is compiled once here as an
# exported C++20 module (its BMI is shared with backend targets that link this lib);
# a header is a plain private source as before.
function(avnd_add_object_to_base base main_file)
  if("${main_file}" MATCHES "\\.cppm$")
    target_sources(${base} PUBLIC FILE_SET CXX_MODULES FILES "${main_file}")
    if(TARGET avnd_halp_module)
      target_link_libraries(${base} PUBLIC avnd_halp_module)
    endif()
  else()
    target_sources(${base} PRIVATE "${main_file}")
  endif()
endfunction()

# Make a backend target consume the user object: for a module, link its base library
# (so the prototype's `import` resolves) instead of recompiling it; for a header,
# compile it into the backend TU as before.
function(avnd_add_object_to_backend backend base main_file)
  if("${main_file}" MATCHES "\\.cppm$")
    target_link_libraries(${backend} PRIVATE ${base})
  else()
    target_sources(${backend} PRIVATE "${main_file}")
  endif()
endfunction()

# The slim halp module, built once and reused by every backend target.
if(CMAKE_CXX_SCAN_FOR_MODULES AND NOT TARGET avnd_halp_module)
  add_library(avnd_halp_module STATIC)
  target_sources(avnd_halp_module
    PUBLIC
      FILE_SET CXX_MODULES
      FILES
        "${AVND_SOURCE_DIR}/include/halp/halp.cppm"
  )
  target_link_libraries(avnd_halp_module PUBLIC Avendish::Avendish)
  target_compile_features(avnd_halp_module PUBLIC cxx_std_23 INTERFACE cxx_std_23)
endif()
