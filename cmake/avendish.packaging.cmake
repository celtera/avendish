# Reusable, CMake-native packaging for multi-object Avendish *addons* (the
# avnd_addon_object / avnd_addon_finalize path). These mirror the per-environment
# layout that the ossia/actions package-<backend>-addon composite actions produced
# in bash, but as callable CMake -- so an addon assembles its distributable
# packages at build time, and CI is only left to zip + upload.
#
# The headline feature over the bash actions is SUPPORT: a list of extra files
# (typically a runtime shared library the objects dlopen at startup, e.g.
# libonnxruntime) bundled into each package at the location that environment's
# loader looks for siblings of a plugin:
#   - Max/MSP      -> <pkg>/support/         (Max adds support/ to the loader path;
#                                             companion dylibs AND dlls go here)
#   - TouchDesigner-> <pkg>/Plugins/         (next to the flat plugin binary)
#   - Godot        -> <pkg>/bin/<os>-<arch>/ (next to the GDExtension binary; also
#                                             declared in the .gdextension [dependencies])
#
# Single-object projects (avnd_make + EXTERNALS lists, e.g. celtera/ultraleap) keep
# using avnd_create_max_package / avnd_create_pd_package in avendish.max/pd.cmake.

# Directory of this module, so POST_BUILD steps can find the helper scripts. Stored
# as a GLOBAL property: a plain variable set here lands in the (find_package) include
# scope and isn't visible when avnd_addon_package is later called from the addon's
# own scope, whereas a global property always is.
set_property(GLOBAL PROPERTY AVND_PACKAGING_DIR "${CMAKE_CURRENT_LIST_DIR}")

# --- helpers ----------------------------------------------------------------

# avnd_packaging_platform_tags(<os-var> <arch-var>): set the os / arch tags used in
# zip names and the Godot bin/<os>-<arch>/ subdir, matching the CI tag scheme
# (linux|macos|windows  ×  x86_64|arm64).
function(avnd_packaging_platform_tags out_os out_arch)
  if(APPLE)
    set(_os "macos")
  elseif(WIN32)
    set(_os "windows")
  else()
    set(_os "linux")
  endif()

  # Prefer the explicitly requested macOS arch; otherwise the build processor.
  set(_arch "${CMAKE_SYSTEM_PROCESSOR}")
  if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(GET CMAKE_OSX_ARCHITECTURES 0 _arch)
  endif()
  string(TOLOWER "${_arch}" _arch)
  if(_arch MATCHES "aarch64|arm64")
    set(_arch "arm64")
  elseif(_arch MATCHES "x86_64|amd64|x64")
    set(_arch "x86_64")
  endif()

  set(${out_os} "${_os}" PARENT_SCOPE)
  set(${out_arch} "${_arch}" PARENT_SCOPE)
endfunction()

# avnd_packaging_copy_metadata(<pkg-dir> <source-path> <addon-name>): copy the
# LICENSE/README variants found in <source-path> into <pkg-dir>, and copy or
# synthesize package-info.json (Max only passes a real one; harmless elsewhere but
# we only call it where it matters).
function(avnd_packaging_copy_metadata pkg source_path addon_name)
  foreach(_meta LICENSE LICENSE.txt LICENSE.md README README.md README.txt)
    if(EXISTS "${source_path}/${_meta}")
      file(COPY "${source_path}/${_meta}" DESTINATION "${pkg}/")
    endif()
  endforeach()
endfunction()

# --- Max/MSP (addon) --------------------------------------------------------
#
# avnd_create_max_addon_package(
#   NAME <addon-name> SOURCE_PATH <dir> PACKAGE_ROOT <dir>
#   EXTERNALS <target> ...      # the *_max external targets
#   [SUPPORT <file> ...])
#
# Distinct from avnd_create_max_package (avendish.max.cmake), which copies a
# curated SOURCE_PATH/* skeleton -- pointing that at an addon's source root would
# recurse into the package dir being built. This addon variant copies only the
# known metadata (LICENSE/README/package-info.json) plus the externals + SUPPORT.
# Layout: <pkg>/externals/<.mxo|.mxe64>  +  <pkg>/support/<bundled libs>  +
#         <pkg>/docs/refpages/<.maxref.xml>  +  package-info.json + LICENSE/README.
function(avnd_create_max_addon_package)
  cmake_parse_arguments(AVND "" "NAME;SOURCE_PATH;PACKAGE_ROOT" "EXTERNALS;SUPPORT" ${ARGN})
  if(NOT AVND_EXTERNALS)
    return()
  endif()

  set(_pkg "${AVND_PACKAGE_ROOT}/${AVND_NAME}")
  set(_pkg_target "${AVND_NAME}_max_package")
  add_custom_target(${_pkg_target} ALL DEPENDS ${AVND_EXTERNALS})

  if(AVND_SOURCE_PATH)
    avnd_packaging_copy_metadata("${_pkg}" "${AVND_SOURCE_PATH}" "${AVND_NAME}")
    if(EXISTS "${AVND_SOURCE_PATH}/package-info.json")
      file(COPY "${AVND_SOURCE_PATH}/package-info.json" DESTINATION "${_pkg}/")
    else()
      file(WRITE "${_pkg}/package-info.json"
"{\n  \"name\": \"${AVND_NAME}\",\n  \"displayname\": \"${AVND_NAME}\",\n  \"version\": \"continuous\",\n  \"author\": \"ossia.io\",\n  \"description\": \"Built from avendish -- see https://github.com/celtera/avendish\",\n  \"homepatcher\": \"\"\n}\n")
    endif()
  endif()

  foreach(_external ${AVND_EXTERNALS})
    add_custom_command(TARGET ${_external} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_pkg}/externals"
      COMMENT "Packaging Max external: ${_external}"
      VERBATIM)

    get_target_property(_maxref ${_external} AVND_MAX_MAXREF_XML)
    if(_maxref)
      # The maxref is produced by a separate dump target (avnd_make_max); depend on
      # it so the .maxref.xml is built first. Copy it best-effort: the generator
      # doesn't emit it on every toolchain (e.g. the Windows multi-config build), and
      # a missing doc must not fail the package build.
      add_dependencies("${_external}" "dump_maxref_${_external}")
      get_property(_pkgdir GLOBAL PROPERTY AVND_PACKAGING_DIR)
      add_custom_command(TARGET ${_external} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
          "-DSRC=${CMAKE_BINARY_DIR}/${_maxref}" "-DDST=${_pkg}/docs/refpages"
          -P "${_pkgdir}/avendish.packaging.copy_optional.cmake"
        VERBATIM)
    endif()

    if(WIN32)
      add_custom_command(TARGET ${_external} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_external}>" "${_pkg}/externals/"
        VERBATIM)
    else()
      # rpath so a LINKED support lib resolves from <pkg>/support/. Harmless for a
      # dlopen'd dep like onnxruntime (found via the loader's module-relative search).
      set_target_properties(${_external} PROPERTIES
        BUILD_WITH_INSTALL_RPATH 1
        INSTALL_NAME_DIR "@rpath"
        BUILD_RPATH "@loader_path/../../../../support"
        INSTALL_RPATH "@loader_path/../../../../support")
      add_custom_command(TARGET ${_external} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_BUNDLE_DIR:${_external}>"
            "${_pkg}/externals/$<TARGET_BUNDLE_DIR_NAME:${_external}>"
        VERBATIM)
    endif()
  endforeach()

  foreach(_support ${AVND_SUPPORT})
    add_custom_command(TARGET ${_pkg_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_pkg}/support"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_support}" "${_pkg}/support/"
      VERBATIM)
  endforeach()
endfunction()

# --- TouchDesigner ----------------------------------------------------------
#
# avnd_create_touchdesigner_package(
#   NAME <addon-name>            # package folder + zip prefix
#   SOURCE_PATH <dir>           # holds LICENSE/README
#   PACKAGE_ROOT <dir>          # output root; package goes in <root>/<name>
#   EXTERNALS <target> ...      # the *_<PTYPE>_td plugin targets
#   [SUPPORT <file> ...])       # extra libs, bundled next to the plugins in Plugins/
#
# Layout: <root>/<name>/Plugins/<flat .dylib|.dll>  +  bundled SUPPORT siblings.
function(avnd_create_touchdesigner_package)
  cmake_parse_arguments(AVND "" "NAME;SOURCE_PATH;PACKAGE_ROOT" "EXTERNALS;SUPPORT" ${ARGN})
  if(NOT AVND_EXTERNALS)
    return()
  endif()

  set(_pkg "${AVND_PACKAGE_ROOT}/${AVND_NAME}")
  set(_plugins "${_pkg}/Plugins")
  set(_pkg_target "${AVND_NAME}_touchdesigner_package")
  add_custom_target(${_pkg_target} ALL DEPENDS ${AVND_EXTERNALS})

  if(AVND_SOURCE_PATH)
    avnd_packaging_copy_metadata("${_pkg}" "${AVND_SOURCE_PATH}" "${AVND_NAME}")
  endif()

  foreach(_external ${AVND_EXTERNALS})
    add_custom_command(TARGET ${_external} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_plugins}"
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_external}>" "${_plugins}/"
      COMMENT "Packaging TouchDesigner plugin: ${_external}"
      VERBATIM)
  endforeach()

  # SUPPORT libs live next to the plugins (the OS loader / our get_module_folder()
  # search resolves siblings of the loaded plugin binary).
  foreach(_support ${AVND_SUPPORT})
    add_custom_command(TARGET ${_pkg_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_plugins}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_support}" "${_plugins}/"
      VERBATIM)
  endforeach()
endfunction()

# --- Godot ------------------------------------------------------------------
#
# avnd_create_godot_package(
#   NAME <addon-name> SOURCE_PATH <dir> PACKAGE_ROOT <dir>
#   EXTERNALS <target> ...      # the *_<PTYPE>_godot targets
#   GDEXTENSIONS <file> ...     # the generated <c_name><suffix>.gdextension files
#   [SUPPORT <file> ...])
#
# Layout: <root>/<name>/<port>.gdextension  +  <root>/<name>/bin/<os>-<arch>/<binary>
#         with SUPPORT libs bundled in the same bin/<os>-<arch>/ folder and declared
#         under each .gdextension's [dependencies].
function(avnd_create_godot_package)
  cmake_parse_arguments(AVND "" "NAME;SOURCE_PATH;PACKAGE_ROOT" "EXTERNALS;GDEXTENSIONS;SUPPORT" ${ARGN})
  if(NOT AVND_EXTERNALS)
    return()
  endif()

  avnd_packaging_platform_tags(_os _arch)
  set(_pkg "${AVND_PACKAGE_ROOT}/${AVND_NAME}")
  set(_bin_sub "bin/${_os}-${_arch}")
  set(_bin "${_pkg}/${_bin_sub}")
  set(_pkg_target "${AVND_NAME}_godot_package")
  add_custom_target(${_pkg_target} ALL DEPENDS ${AVND_EXTERNALS})

  if(AVND_SOURCE_PATH)
    avnd_packaging_copy_metadata("${_pkg}" "${AVND_SOURCE_PATH}" "${AVND_NAME}")
  endif()

  # Copy the built binary (a .framework bundle dir on macOS, a flat .so/.dll else)
  # into bin/<os>-<arch>/.
  foreach(_external ${AVND_EXTERNALS})
    get_target_property(_is_fw ${_external} FRAMEWORK)
    if(_is_fw)
      set(_copy COMMAND ${CMAKE_COMMAND} -E copy_directory
          "$<TARGET_BUNDLE_DIR:${_external}>"
          "${_bin}/$<TARGET_BUNDLE_DIR_NAME:${_external}>")
    else()
      set(_copy COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_external}>" "${_bin}/")
    endif()
    add_custom_command(TARGET ${_external} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_bin}"
      ${_copy}
      COMMENT "Packaging Godot GDExtension: ${_external}"
      VERBATIM)
  endforeach()

  # Rewrite each .gdextension so its [libraries] res:// paths point at
  # addons/<name>/bin/<os>-<arch>/<basename> (the unioned, per-OS install layout),
  # and append a [dependencies] entry for every SUPPORT lib so Godot's exporter
  # copies them alongside. Done at configure time (the manifests already exist).
  foreach(_gd ${AVND_GDEXTENSIONS})
    get_filename_component(_gd_name "${_gd}" NAME)
    set(_out "${_pkg}/${_gd_name}")
    avnd_packaging_rewrite_gdextension("${_gd}" "${_out}" "${AVND_NAME}" "${_os}" "${_arch}" "${_bin_sub}" "${AVND_SUPPORT}")
  endforeach()

  foreach(_support ${AVND_SUPPORT})
    add_custom_command(TARGET ${_pkg_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_bin}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_support}" "${_bin}/"
      VERBATIM)
  endforeach()
endfunction()

# Rewrite a single .gdextension manifest: repoint [libraries] res:// values at
# res://addons/<name>/<bin-sub>/<basename> and add a [dependencies] block listing
# the SUPPORT libs (so Godot copies them next to the extension binary on export).
function(avnd_packaging_rewrite_gdextension src dst addon_name os arch bin_sub support)
  file(READ "${src}" _content)

  # Repoint every [libraries] res:// path at addons/<name>/bin/<os>-<arch>/<file>,
  # deriving <os>-<arch> from each entry's OWN platform key (not this build's), so
  # the one manifest references every platform -- which is what lets the per-OS
  # packages union into a single addons/<name>/ with side-by-side bin/<os>-<arch>/.
  #   key forms:  <plat>.<debug|release>.<arch> = "..."   or   macos.<debug|release> = "..."
  # Pass A: keys carrying an explicit arch -> bin/<plat>-<arch>/.
  string(REGEX REPLACE
    "([ \t]*)(linux|macos|windows)\\.(debug|release)\\.([A-Za-z0-9_]+)([ \t]*=[ \t]*\")res://[^\"]*/([^/\"]+)\""
    "\\1\\2.\\3.\\4\\5res://addons/${addon_name}/bin/\\2-\\4/\\6\""
    _content "${_content}")
  # Pass B: the arch-less macos key (universal) -> bin/macos-arm64/ (matches the
  # default the CI bash packager used; arm64 is the standard macOS Godot target).
  string(REGEX REPLACE
    "([ \t]*)macos\\.(debug|release)([ \t]*=[ \t]*\")res://[^\"]*/([^/\"]+)\""
    "\\1macos.\\2\\3res://addons/${addon_name}/bin/macos-arm64/\\4\""
    _content "${_content}")

  # Declare the bundled SUPPORT libs as [dependencies] so Godot's exporter copies
  # them next to the extension binary. Only the current build's platform/arch is
  # known here (we have just this OS's filenames), so we emit that platform's keys.
  # Runtime loading does NOT rely on this -- the libs sit beside the binary and are
  # found via the loader's module-relative search -- it only helps `godot export`.
  if(support AND NOT _content MATCHES "\\[dependencies\\]")
    string(APPEND _content "\n[dependencies]\n")
    foreach(_s ${support})
      get_filename_component(_sn "${_s}" NAME)
      string(APPEND _content
        "${os}.debug.${arch} = { \"res://addons/${addon_name}/${bin_sub}/${_sn}\" : \"\" }\n")
      string(APPEND _content
        "${os}.release.${arch} = { \"res://addons/${addon_name}/${bin_sub}/${_sn}\" : \"\" }\n")
    endforeach()
  endif()

  get_filename_component(_dstdir "${dst}" DIRECTORY)
  file(MAKE_DIRECTORY "${_dstdir}")
  file(WRITE "${dst}" "${_content}")
endfunction()

# --- addon orchestrator -----------------------------------------------------
#
# avnd_addon_package(
#   NAME <addon-name>           # package folder + zip prefix (e.g. score-addon-onnx)
#   SOURCE_PATH <dir>           # LICENSE/README/help/package-info.json
#   PACKAGE_ROOT <dir>          # output root
#   BACKENDS max touchdesigner godot ...   # which packages to assemble
#   [SUPPORT <file> ...])       # libs to bundle into each package (per-env placement)
#
# Reads the per-backend external targets that avnd_make_* registered globally while
# the addon's avnd_addon_object() calls ran, and assembles one package per backend.
function(avnd_addon_package)
  cmake_parse_arguments(AVND "" "NAME;SOURCE_PATH;PACKAGE_ROOT" "BACKENDS;SUPPORT" ${ARGN})
  if(NOT AVND_PACKAGE_ROOT)
    set(AVND_PACKAGE_ROOT "${CMAKE_BINARY_DIR}/package")
  endif()

  foreach(_backend ${AVND_BACKENDS})
    string(TOLOWER "${_backend}" _backend)
    if(_backend STREQUAL "td")
      set(_backend "touchdesigner")
    endif()
    string(TOUPPER "${_backend}" _BK)
    get_property(_externals GLOBAL PROPERTY AVND_ALL_${_BK}_EXTERNALS)
    if(NOT _externals)
      message(STATUS "avnd_addon_package: no ${_backend} externals registered, skipping")
      continue()
    endif()

    if(_backend STREQUAL "max")
      avnd_create_max_addon_package(
        NAME "${AVND_NAME}"
        SOURCE_PATH "${AVND_SOURCE_PATH}"
        PACKAGE_ROOT "${AVND_PACKAGE_ROOT}"
        EXTERNALS ${_externals}
        SUPPORT ${AVND_SUPPORT})
    elseif(_backend STREQUAL "touchdesigner")
      avnd_create_touchdesigner_package(
        NAME "${AVND_NAME}"
        SOURCE_PATH "${AVND_SOURCE_PATH}"
        PACKAGE_ROOT "${AVND_PACKAGE_ROOT}"
        EXTERNALS ${_externals}
        SUPPORT ${AVND_SUPPORT})
    elseif(_backend STREQUAL "godot")
      get_property(_gdext GLOBAL PROPERTY AVND_ALL_GODOT_GDEXTENSIONS)
      avnd_create_godot_package(
        NAME "${AVND_NAME}"
        SOURCE_PATH "${AVND_SOURCE_PATH}"
        PACKAGE_ROOT "${AVND_PACKAGE_ROOT}"
        EXTERNALS ${_externals}
        GDEXTENSIONS ${_gdext}
        SUPPORT ${AVND_SUPPORT})
    else()
      message(STATUS "avnd_addon_package: backend '${_backend}' has no packager, skipping")
    endif()
  endforeach()
endfunction()
