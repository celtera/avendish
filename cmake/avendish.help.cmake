# Help / example patch generation.
#
# For a given backend, ship either an explicit per-object override file (copied
# verbatim) or, failing that, an auto-generated patch produced by the
# `generate_patches` tool from the object's dump JSON. Mirrors the maxref wiring
# in avendish.max.cmake.
#
# avnd_generate_help(
#   FX_TARGET     <module-target>     # backend target, owns the result property
#   SOURCE_TARGET <object-target>     # carries AVND_DUMP_PATH
#   BACKEND       <pd|max|godot|td|python>
#   DESTINATION   <output-path>       # may contain $<CONFIG> generator exprs
#   PROPERTY      <AVND_..._HELP>     # property set to DESTINATION on FX_TARGET
#   [OVERRIDE     <file>]             # explicit hand-authored file
# )
function(avnd_generate_help)
  cmake_parse_arguments(AVND ""
    "FX_TARGET;SOURCE_TARGET;BACKEND;DESTINATION;PROPERTY;OVERRIDE;EXTRA_ARG" "" ${ARGN})

  if(AVND_DISABLE_AUTOHELP)
    return()
  endif()

  # 1) Explicit per-object override: copy it verbatim to the expected place.
  if(AVND_OVERRIDE)
    if(NOT IS_ABSOLUTE "${AVND_OVERRIDE}")
      set(AVND_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/${AVND_OVERRIDE}")
    endif()
    if(EXISTS "${AVND_OVERRIDE}")
      add_custom_target(${AVND_FX_TARGET}_help ALL
          ${CMAKE_COMMAND} -E copy_if_different
            "${AVND_OVERRIDE}" "${AVND_DESTINATION}"
          DEPENDS "${AVND_OVERRIDE}"
          BYPRODUCTS "${AVND_DESTINATION}"
        )
      set_target_properties(${AVND_FX_TARGET}
        PROPERTIES "${AVND_PROPERTY}" "${AVND_DESTINATION}")
      return()
    else()
      message(WARNING
        "avnd_generate_help: override '${AVND_OVERRIDE}' not found for "
        "${AVND_FX_TARGET} (${AVND_BACKEND}); falling back to auto-generation")
    endif()
  endif()

  # 2) Auto-generate from the dump JSON.
  if(NOT TARGET generate_patches)
    return()
  endif()
  if(NOT TARGET ${AVND_SOURCE_TARGET})
    return()
  endif()
  get_target_property(_dump_path ${AVND_SOURCE_TARGET} AVND_DUMP_PATH)
  if(NOT _dump_path)
    return()
  endif()

  add_custom_target(${AVND_FX_TARGET}_help ALL
      generate_patches "${AVND_BACKEND}" "${_dump_path}" "${AVND_DESTINATION}" ${AVND_EXTRA_ARG}
      DEPENDS "${_dump_path}" generate_patches
      BYPRODUCTS "${AVND_DESTINATION}"
    )
  set_target_properties(${AVND_FX_TARGET}
    PROPERTIES "${AVND_PROPERTY}" "${AVND_DESTINATION}")
endfunction()
