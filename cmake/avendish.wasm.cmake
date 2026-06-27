# Skip this back-end's setup (and its dependencies) entirely when it is disabled,
# so a single-back-end build -- e.g. one avnd-addon CI lane that passes
# -DAVND_ENABLE_WASM=OFF -- doesn't fetch/build what it won't use (godot-cpp, the
# VST3 SDK, ...). Matches _avnd_dispatch_backend: only act when explicitly OFF.
if(DEFINED AVND_ENABLE_WASM AND NOT AVND_ENABLE_WASM)
  return()
endif()

# SPDX-License-Identifier: GPL-3.0-or-later
# WebAssembly / Web backend.

if(NOT EMSCRIPTEN)
  function(avnd_make_wasm)
  endfunction()
  return()
endif()

# Cached so avnd_make_wasm (and the packaging helpers) still see it when called
# from another directory scope -- e.g. an addon's avnd_addon_object, where
# avendish is add_subdirectory'd and this file-scope set() would otherwise be
# invisible, silently skipping all the configure_file/copy steps.
set(AVND_WASM_JS_DIR "${AVND_SOURCE_DIR}/include/avnd/binding/wasm/js" CACHE INTERNAL "")

set(AVND_WASM_COMMON_LINK_FLAGS
  "-sWASM=1"
  "-sMODULARIZE=1"
  "-sEXPORT_ES6=1"
  # web+worker for browser/AudioWorklet, node for headless mode.
  "-sENVIRONMENT=web,worker,node"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sEXPORTED_RUNTIME_METHODS=['HEAPF32','HEAPU8','HEAP32','HEAPU32','HEAPF64']"
  "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
)

set(AVND_WASM_COMMON_COMPILE_FLAGS
  "-msimd128"
  "-matomics"
  "-mbulk-memory"
  # fmt 12 header-only calls bare malloc/free without including <cstdlib>, which
  # emscripten's libc++ doesn't transitively pull in ; force-include it.
  "-include" "cstdlib"
)

# Embind needs RTTI, so keep it ON but still disable exceptions (hence we don't
# link DisableExceptions, which would add -fno-rtti).
set(AVND_WASM_NOEXCEPT_COMPILE_FLAGS
  "-fno-exceptions"
)
set(AVND_WASM_NOEXCEPT_DEFINITIONS
  "_LIBCPP_NO_EXCEPTIONS=1"
  "BOOST_NO_EXCEPTIONS=1"
)

function(avnd_make_wasm)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  set(AVND_FX_TARGET "${AVND_TARGET}_wasm")
  if(TARGET "${AVND_FX_TARGET}")
    # Both avnd_make_object and avnd_make_audioplug invoke us; guard against it.
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/wasm/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_wasm.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  add_executable(${AVND_FX_TARGET})

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      RUNTIME_OUTPUT_DIRECTORY wasm
      # .mjs so Node treats it as a module without package.json.
      SUFFIX ".mjs"
      AVND_C_NAME "${AVND_C_NAME}"
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_wasm.cpp"
  )

  target_compile_definitions(
    ${AVND_FX_TARGET}
    PRIVATE
      AVND_WEBASSEMBLY=1
      ${AVND_WASM_NOEXCEPT_DEFINITIONS}
  )

  target_compile_options(${AVND_FX_TARGET} PRIVATE
    ${AVND_WASM_COMMON_COMPILE_FLAGS}
    ${AVND_WASM_NOEXCEPT_COMPILE_FLAGS}
  )
  target_link_options(${AVND_FX_TARGET} PRIVATE
    ${AVND_WASM_COMMON_LINK_FLAGS}
    ${AVND_WASM_COMMON_COMPILE_FLAGS}
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish_wasm
      # embind must be linked AFTER the objects that reference it: modern
      # emscripten links it via -lembind (the old --bind alias no longer pulls
      # it in), and with --gc-sections it gets stripped unless it follows the
      # objects in the link line — hence target_link_libraries, not link_options.
      "-lembind"
  )

  # worklet + standalone page
  _avnd_wasm_generate_packaging("${AVND_FX_TARGET}" "${AVND_C_NAME}")

  # WAM2 package in its own subdir so it can be served as a self-contained WAM.
  _avnd_wasm_generate_wam_packaging("${AVND_FX_TARGET}" "${AVND_C_NAME}")

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  # Register in the gallery index ("c_name|display_name").
  set(_display "${AVND_MAIN_CLASS}")
  set_property(GLOBAL APPEND PROPERTY AVND_WASM_GALLERY_ENTRIES
    "${AVND_C_NAME}|${_display}")
  if(TARGET wasm_gallery)
    add_dependencies(wasm_gallery ${AVND_FX_TARGET})
  endif()
endfunction()

# Emits the JS wrapper files next to the built module.
function(_avnd_wasm_generate_packaging fx_target c_name)
  set(_outdir "${CMAKE_BINARY_DIR}/wasm")

  set(AVND_WASM_C_NAME "${c_name}")
  if(NOT DEFINED AVND_WASM_NAME OR AVND_WASM_NAME STREQUAL "")
    set(AVND_WASM_NAME "${c_name}")
  endif()

  # Output names MUST use dashes: the JS runtime cross-references them as
  # '<c_name>-worklet.js' / '<c_name>-standalone.js'. The page is per-plugin
  # (<c_name>.html) so multiple plug-ins don't collide in one wasm/ dir.
  foreach(tpl
      "worklet.js.in:${c_name}-worklet.js"
      "standalone.html.in:${c_name}.html"
      "standalone.js.in:${c_name}-standalone.js")
    string(REPLACE ":" ";" _pair "${tpl}")
    list(GET _pair 0 _src)
    list(GET _pair 1 _dst)
    if(EXISTS "${AVND_WASM_JS_DIR}/${_src}")
      configure_file(
        "${AVND_WASM_JS_DIR}/${_src}"
        "${_outdir}/${_dst}"
        @ONLY
        NEWLINE_STYLE LF
      )
    endif()
  endforeach()

  # Copy the static shared runtime next to the built module.
  foreach(_static
      "avnd-ringbuf.js"
      "avnd-audio-helper.js"
      "avnd-dsp.js"
      "avnd-ui.js"
      "avnd-node-runner.mjs"
      "avnd-dev-server.mjs")
    if(EXISTS "${AVND_WASM_JS_DIR}/${_static}")
      add_custom_command(TARGET ${fx_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${AVND_WASM_JS_DIR}/${_static}" "${_outdir}/${_static}"
      )
    endif()
  endforeach()
endfunction()

# Web Audio Modules 2 (WAM2) packaging: wraps the Embind module as a
# self-contained WAM under wasm/wam/<c_name>/. Templates in js/wam/.
function(_avnd_wasm_generate_wam_packaging fx_target c_name)
  set(_wamsrc "${AVND_WASM_JS_DIR}/wam")
  set(_wamout "${CMAKE_BINARY_DIR}/wasm/wam/${c_name}")

  set(AVND_WASM_C_NAME "${c_name}")

  # Configure-time defaults; index.js self-corrects name/vendor/version from the
  # module's static getters at load time.
  if(NOT DEFINED AVND_WASM_NAME OR AVND_WASM_NAME STREQUAL "")
    set(AVND_WASM_NAME "${c_name}")
  endif()
  if(NOT DEFINED AVND_WASM_VENDOR OR AVND_WASM_VENDOR STREQUAL "")
    set(AVND_WASM_VENDOR "Avendish")
  endif()
  if(NOT DEFINED AVND_WASM_VERSION OR AVND_WASM_VERSION STREQUAL "")
    set(AVND_WASM_VERSION "1.0.0")
  endif()
  if(NOT DEFINED AVND_WASM_VENDOR_URL OR AVND_WASM_VENDOR_URL STREQUAL "")
    set(AVND_WASM_VENDOR_URL "https://github.com/celtera/avendish")
  endif()

  # Templated WAM files (strip the trailing .in).
  foreach(tpl
      "index.js.in:index.js"
      "node.js.in:node.js"
      "sdk-wamnode.js.in:sdk-wamnode.js"
      "processor.js.in:processor.js"
      "gui.js.in:gui.js"
      "descriptor.json.in:descriptor.json"
      "package.json.in:package.json")
    string(REPLACE ":" ";" _pair "${tpl}")
    list(GET _pair 0 _src)
    list(GET _pair 1 _dst)
    if(EXISTS "${_wamsrc}/${_src}")
      configure_file(
        "${_wamsrc}/${_src}"
        "${_wamout}/${_dst}"
        @ONLY
        NEWLINE_STYLE LF
      )
    endif()
  endforeach()

  # Plain (non-templated) shared WAM helpers copied verbatim.
  foreach(_static
      "avnd-wam-params.js"
      "README.md")
    if(EXISTS "${_wamsrc}/${_static}")
      configure_file(
        "${_wamsrc}/${_static}"
        "${_wamout}/${_static}"
        COPYONLY
      )
    endif()
  endforeach()

  # Copy the built module + wasm into the WAM dir so it's self-contained.
  add_custom_command(TARGET ${fx_target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_wamout}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/wasm/${c_name}.mjs" "${_wamout}/${c_name}.mjs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/wasm/${c_name}.wasm" "${_wamout}/${c_name}.wasm"
    VERBATIM
  )
endfunction()

add_library(Avendish_wasm INTERFACE)
# NB: NOT linking DisableExceptions (it forces -fno-rtti, which Embind needs).
# Exceptions are disabled via per-target flags in avnd_make_wasm instead.
target_link_libraries(Avendish_wasm INTERFACE Avendish)
add_library(Avendish::Avendish_wasm ALIAS Avendish_wasm)

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/wasm/all.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/wasm/audio_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/wasm/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/wasm/module.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/wasm/ringbuffer.hpp"
)

# Convenience targets: wasm_gallery (build all + landing page) and wasm_serve
# (serve wasm/ with COOP/COEP isolation headers, on :8088).
set(AVND_WASM_OUT_DIR "${CMAKE_BINARY_DIR}/wasm")

add_custom_target(wasm_gallery COMMENT "Building the Avendish WASM gallery")

# DEFER so the index generator sees the complete entries list, after all
# avnd_make_wasm() calls have run.
cmake_language(DEFER CALL _avnd_wasm_finalize_gallery)

function(_avnd_wasm_finalize_gallery)
  get_property(_entries GLOBAL PROPERTY AVND_WASM_GALLERY_ENTRIES)
  add_custom_command(TARGET wasm_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      "-DGALLERY_ENTRIES=${_entries}"
      "-DOUT_DIR=${AVND_WASM_OUT_DIR}"
      -P "${AVND_SOURCE_DIR}/cmake/avendish.wasm.gallery.cmake"
    COMMENT "Generating wasm/index.html"
    VERBATIM
  )
endfunction()

# Serve the built gallery with COOP/COEP. Uses the shipped node dev server.
find_program(AVND_NODE_EXECUTABLE NAMES node nodejs)
if(AVND_NODE_EXECUTABLE)
  add_custom_target(wasm_serve
    COMMAND ${AVND_NODE_EXECUTABLE}
      "${AVND_WASM_JS_DIR}/avnd-dev-server.mjs"
      "${AVND_WASM_OUT_DIR}" --port 8088
    USES_TERMINAL
    COMMENT "Serving ${AVND_WASM_OUT_DIR} on http://localhost:8088 (COOP/COEP)"
  )
  add_dependencies(wasm_serve wasm_gallery)
endif()
