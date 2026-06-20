# The `halp` C++20 module (avnd_halp_module). Lets user objects `import halp;`
# instead of including the heavy halp headers; built once and reused by every
# backend target. Only available when the toolchain/generator scans for modules.
if(CMAKE_CXX_SCAN_FOR_MODULES AND NOT TARGET avnd_halp_module)
  add_library(avnd_halp_module STATIC)
  target_sources(avnd_halp_module
    PUBLIC
      FILE_SET CXX_MODULES
      FILES
        "${AVND_SOURCE_DIR}/include/halp/halp.cppm"
  )
  # Avendish carries the avnd/halp include dirs plus boost / fmt / magic_enum.
  target_link_libraries(avnd_halp_module PUBLIC Avendish::Avendish)
  target_compile_features(avnd_halp_module PUBLIC cxx_std_23 INTERFACE cxx_std_23)
endif()
