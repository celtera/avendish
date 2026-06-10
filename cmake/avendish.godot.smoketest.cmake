# Headless smoke test for an avendish Godot GDExtension.
#
# Builds a minimal Godot project around the built library + .gdextension,
# starts `godot --headless` on it and asserts that the extension loads and
# registers its class. Meant to be driven by the test registered in
# avnd_make_godot (avendish.godot.cmake), but any avendish-based plugin or
# template can invoke it directly:
#
#   cmake -DGODOT=/path/to/godot
#         -DLIBRARY=/path/to/build/godot/my_plugin.so
#         -DGDEXTENSION=/path/to/build/godot/my_plugin.gdextension
#         -DCLASS=avnd_my_plugin
#         -DWORK_DIR=/tmp/smoke
#         -P avendish.godot.smoketest.cmake
#
# Notes:
# - Godot exits with 0 even when an extension fails to load, so the script
#   also scans the output for ERROR markers.
# - Extensions are normally discovered by the editor import step which
#   writes .godot/extension_list.cfg; we write that file directly so that
#   the runtime --headless pass loads the extension without an import pass.

foreach(var GODOT LIBRARY GDEXTENSION CLASS WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "avendish godot smoke test: missing -D${var}=")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/bin" "${WORK_DIR}/.godot")

get_filename_component(_gdext_name "${GDEXTENSION}" NAME)
file(COPY "${LIBRARY}" DESTINATION "${WORK_DIR}/bin")
file(COPY "${GDEXTENSION}" DESTINATION "${WORK_DIR}")

file(WRITE "${WORK_DIR}/project.godot" "config_version=5

[application]
config/name=\"avnd_smoke\"
")

file(WRITE "${WORK_DIR}/.godot/extension_list.cfg" "res://${_gdext_name}
")

file(WRITE "${WORK_DIR}/smoke.gd" "extends SceneTree
func _initialize():
    var cls := \"${CLASS}\"
    if ClassDB.class_exists(cls):
        print(\"AVND_SMOKE_OK: \", cls)
        quit(0)
    else:
        printerr(\"AVND_SMOKE_FAIL: class not registered: \", cls)
        quit(1)
")

execute_process(
  COMMAND "${GODOT}" --headless --path "${WORK_DIR}" -s res://smoke.gd
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err
  TIMEOUT 120
)

set(_all "${_out}\n${_err}")
if(NOT _result EQUAL 0)
  message(FATAL_ERROR "avendish godot smoke test failed (exit ${_result}) for ${LIBRARY}:\n${_all}")
endif()
if(_all MATCHES "ERROR:|SCRIPT ERROR|AVND_SMOKE_FAIL")
  message(FATAL_ERROR "avendish godot smoke test: errors while loading ${LIBRARY}:\n${_all}")
endif()
if(NOT _all MATCHES "AVND_SMOKE_OK")
  message(FATAL_ERROR "avendish godot smoke test: smoke script did not run for ${LIBRARY}:\n${_all}")
endif()
message(STATUS "avendish godot smoke test passed: ${CLASS}")
