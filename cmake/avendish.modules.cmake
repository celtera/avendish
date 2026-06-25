# C++20 module support.
#
# A user object can be written as a module (`export module <c_name>; import halp;`)
# in a .cppm MAIN_FILE instead of a header. The per-backend prototype then either
# `#include`s the header or `import`s the module -- only that one line differs.
#
# GCC requires a module BMI's exception/rtti dialect to match every consumer
# exactly (a mismatch is a hard "Bad file data" error), and avendish backends
# disagree: dump/python/ossia build with exceptions+rtti, while the plugin
# backends (clap/pd/max/vst3/vintage/gstreamer/wasm/example) build with
# -fno-exceptions -fno-rtti. So a single shared halp BMI cannot serve all of
# them. Instead the slim halp module and the user object are compiled *into each
# backend target*, inheriting that backend's dialect. Both are one small object,
# so the duplication is cheap and every backend stays self-contained.
#
# Requirements for the module path: a generator with module support (Ninja --
# the Makefiles generator drops -fmodules) and -DCMAKE_CXX_SCAN_FOR_MODULES=ON
# so the generated prototype's `import` is scanned for its BMI dependency.

# Line a prototype uses to pull in the user object.
function(avnd_main_import out main_file c_name)
  if("${main_file}" MATCHES "\\.cppm$")
    set("${out}" "import ${c_name};" PARENT_SCOPE)
  else()
    set("${out}" "#include <${main_file}>" PARENT_SCOPE)
  endif()
endfunction()

# Add the user object to its per-object base library. A header compiles once here
# as before; a .cppm is compiled per-backend instead (see below), so there is
# nothing to add to the base for the module case.
function(avnd_add_object_to_base base main_file)
  if(NOT "${main_file}" MATCHES "\\.cppm$")
    target_sources(${base} PRIVATE "${main_file}")
  endif()
endfunction()

# Make a backend target consume the user object. For a module, compile the slim
# halp module and the user .cppm into the backend itself, in the backend's own
# dialect, so the prototype's `import` resolves against a BMI built with matching
# exception/rtti flags. For a header, compile it into the backend TU as before.
function(avnd_add_object_to_backend backend base main_file)
  if("${main_file}" MATCHES "\\.cppm$")
    # The halp module lives in the avendish tree, the user object wherever the
    # caller put it; a CXX_MODULES file set requires every file under one of its
    # base dirs, so cover both.
    get_filename_component(_avnd_mf_dir "${main_file}" DIRECTORY)
    target_sources(${backend}
      PRIVATE
        FILE_SET avnd_modules TYPE CXX_MODULES
        BASE_DIRS "${AVND_SOURCE_DIR}/include/halp" "${_avnd_mf_dir}"
        FILES
          "${AVND_SOURCE_DIR}/include/halp/halp.cppm"
          "${main_file}"
    )
    target_compile_features(${backend} PRIVATE cxx_std_23)
  else()
    target_sources(${backend} PRIVATE "${main_file}")
  endif()
endfunction()
