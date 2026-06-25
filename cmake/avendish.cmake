include(CMakeFindDependencyMacro)

set(CMAKE_INCLUDE_CURRENT_DIR 1)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

# Per-backend enable options. Default ON so existing setups don't break.
# CI can scope a build to one backend by turning the others off, e.g.
#   -DAVND_ENABLE_PD=ON -DAVND_ENABLE_MAX=OFF -DAVND_ENABLE_VST3=OFF ...
# These gate the avnd_make_<backend> calls issued by the category presets
# (avnd_make_object, avnd_make_audioplug, ...) and by the explicit
# avnd_addon_object(BACKENDS ...) form. They do not affect backends that
# already self-skip when their SDK / dependency is absent.
option(AVND_ENABLE_PD            "Enable Pure Data backend"      ON)
option(AVND_ENABLE_MAX           "Enable Max/MSP backend"        ON)
option(AVND_ENABLE_VST3          "Enable VST3 backend"           ON)
option(AVND_ENABLE_CLAP          "Enable CLAP backend"           ON)
option(AVND_ENABLE_VINTAGE       "Enable VST 2.4 backend"        ON)
option(AVND_ENABLE_TOUCHDESIGNER "Enable TouchDesigner backend"  ON)
option(AVND_ENABLE_PYTHON        "Enable Python backend"         ON)
option(AVND_ENABLE_GODOT         "Enable Godot GDExtension"      ON)
option(AVND_ENABLE_STANDALONE    "Enable standalone executable"  ON)
option(AVND_ENABLE_GSTREAMER     "Enable GStreamer backend"      ON)
option(AVND_ENABLE_WASM          "Enable WebAssembly backend"    ON)
option(AVND_ENABLE_FUZZ          "Enable libFuzzer harness (Clang only)" OFF)

# Opt-in for add-ons that wrap a library whose API requires exceptions/RTTI (e.g.
# onnxruntime). When ON, the DisableExceptions target becomes a no-op so the
# back-ends that link it (pd / max / vst3 / example host / ...) build with
# exceptions enabled. See cmake/avendish.disableexceptions.cmake.
option(AVND_USES_EXCEPTIONS      "Add-on requires C++ exceptions/RTTI"   OFF)

# Max/MSP externals link the static CRT (/MT) on Windows, as the official
# max-sdk-base enforces globally. Set it here -- before the dependencies and the
# addon's object library are created -- so everything linked into the external
# uses the same runtime; mixing /MT and /MD trips MSVC with LNK2038. Scoped to
# when the Max backend is actually enabled.
if(MSVC AND AVND_ENABLE_MAX)
  cmake_policy(SET CMP0091 NEW)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Helper: dispatch to avnd_make_<backend>(...) only if AVND_ENABLE_<BACKEND>
# is ON. Used by the category presets below and by the standalone BACKENDS
# form in AvendishAddon.cmake.
function(_avnd_dispatch_backend backend)
  string(TOUPPER "${backend}" _upper)
  set(_flag "AVND_ENABLE_${_upper}")
  # Backends not covered by a flag (dump, ossia, example_host) always dispatch.
  if(DEFINED ${_flag} AND NOT ${${_flag}})
    return()
  endif()
  # Compute the prototype's object-pull-in line once; the called backend's
  # configure_file picks up AVND_MAIN_IMPORT from this (calling) scope.
  cmake_parse_arguments(AVND "" "MAIN_FILE;C_NAME" "" ${ARGN})
  avnd_main_import(AVND_MAIN_IMPORT "${AVND_MAIN_FILE}" "${AVND_C_NAME}")
  cmake_language(CALL "avnd_make_${backend}" ${ARGN})
endfunction()

find_package(Boost 1.87 QUIET REQUIRED)
find_package(Threads QUIET)
find_package(fmt QUIET)


set(AVENDISH_SOURCES
    "${AVND_SOURCE_DIR}/include/avnd/concepts/all.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/attributes.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/audio_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/audio_processor.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/callback.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/channels.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/control.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/curve.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/dynamic_ports.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/fft.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/field_names.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/file_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/generic.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/gfx.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/gpu_compute.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/hardware.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/init.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/layout.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/logger.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/mapper.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/message.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/message_bus.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/midi.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/midi_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/midifile.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/modules.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/object.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/painter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/parameter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/processor.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/range.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/schedule.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/smooth.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/soundfile.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/synth.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/temporality.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/tensor.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/ui.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/widget.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/worker.hpp"

    "${AVND_SOURCE_DIR}/include/avnd/introspection/channels.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/field_names.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/field_names.p2996.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/generic.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/gfx.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/hardware.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/input.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/layout.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/mapper.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/messages.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/messages.p2996.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/metadata.p2996.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/midi.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/modules.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/output.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/range.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/smooth.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/type_wrapper.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/vecf.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/widgets.hpp"


    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/base.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/per_channel_arg.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/per_channel_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/per_frame_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/per_sample_arg.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/per_sample_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/poly_arg.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process/poly_port.hpp"

    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus/base.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus/per_channel_arg.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus/per_channel_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus/per_sample_arg.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus/per_sample_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus/poly_arg.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus/poly_port.hpp"

    "${AVND_SOURCE_DIR}/include/avnd/wrappers/audio_channel_manager.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/avnd.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/bus_host_process_adapter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/callbacks_adapter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/colors.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/configure.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/control_display.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls_double.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls_fp.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls_storage.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/effect_container.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/generic.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/metadatas.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/prepare.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_adapter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_bus_adapter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_execution.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/ranges.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/smooth.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/soundfile_storage.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/tensor_shim.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/widgets.hpp"

    "${AVND_SOURCE_DIR}/include/avnd/common/arithmetic.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.base.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.includes.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.pfr.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.recursive.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.simple.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.structured.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/array.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/concepts_polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/coroutines.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/dummy.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/errors.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/enum_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/enum_reflection.magic_enum.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/enum_reflection.p2996.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/enums.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/export.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/for_nth.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/function_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/function_reflection.classic.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/function_reflection.p2996.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/index.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/index_sequence.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/inline.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/limited_string.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/limited_string_view.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/member_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/message_tag.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/meta_polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/metadata_tag.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/no_unique_address.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/optionals.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/span_polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/struct_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/tag.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/tuple.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/widechar.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/init_cpp_with_c_object_header.hpp"

    "${AVND_SOURCE_DIR}/include/halp/attributes.hpp"
    "${AVND_SOURCE_DIR}/include/halp/audio.hpp"
    "${AVND_SOURCE_DIR}/include/halp/buffer.hpp"
    "${AVND_SOURCE_DIR}/include/halp/callback.hpp"
    "${AVND_SOURCE_DIR}/include/halp/compat/gamma.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.basic.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.buttons.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.enums.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.sliders.gcc10.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.sliders.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.typedefs.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls_fmt.hpp"
    "${AVND_SOURCE_DIR}/include/halp/curve.hpp"
    "${AVND_SOURCE_DIR}/include/halp/custom_widgets.hpp"
    "${AVND_SOURCE_DIR}/include/halp/device.hpp"
    "${AVND_SOURCE_DIR}/include/halp/device_compute.hpp"
    "${AVND_SOURCE_DIR}/include/halp/dynamic_port.hpp"
    "${AVND_SOURCE_DIR}/include/halp/fft.hpp"
    "${AVND_SOURCE_DIR}/include/halp/file_port.hpp"
    "${AVND_SOURCE_DIR}/include/halp/geometry.hpp"
    "${AVND_SOURCE_DIR}/include/halp/gradient_port.hpp"
    "${AVND_SOURCE_DIR}/include/halp/inline.hpp"
    "${AVND_SOURCE_DIR}/include/halp/layout.hpp"
    "${AVND_SOURCE_DIR}/include/halp/log.hpp"
    "${AVND_SOURCE_DIR}/include/halp/mappers.hpp"
    "${AVND_SOURCE_DIR}/include/halp/messages.hpp"
    "${AVND_SOURCE_DIR}/include/halp/meta.hpp"
    "${AVND_SOURCE_DIR}/include/halp/midi.hpp"
    "${AVND_SOURCE_DIR}/include/halp/midifile_port.hpp"
    "${AVND_SOURCE_DIR}/include/halp/modules.hpp"
    "${AVND_SOURCE_DIR}/include/halp/polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/halp/reactive_value.hpp"
    "${AVND_SOURCE_DIR}/include/halp/sample_accurate_controls.hpp"
    "${AVND_SOURCE_DIR}/include/halp/schedule.hpp"
    "${AVND_SOURCE_DIR}/include/halp/shared_instance.hpp"
    "${AVND_SOURCE_DIR}/include/halp/smooth_controls.hpp"
    "${AVND_SOURCE_DIR}/include/halp/smoothers.hpp"
    "${AVND_SOURCE_DIR}/include/halp/soundfile_port.hpp"
    "${AVND_SOURCE_DIR}/include/halp/static_string.hpp"
    "${AVND_SOURCE_DIR}/include/halp/tensor_port.hpp"
    "${AVND_SOURCE_DIR}/include/halp/texture.hpp"
    "${AVND_SOURCE_DIR}/include/halp/texture_formats.hpp"
    "${AVND_SOURCE_DIR}/include/halp/value_types.hpp"

    "${AVND_SOURCE_DIR}/include/gpp/commands.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/generators.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/meta.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/layout.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/ports.hpp"
)

add_library(avnd_dummy_lib OBJECT "${AVND_SOURCE_DIR}/src/dummy.cpp")

include(avendish.dependencies)
include(avendish.disableexceptions)
include(avendish.sources)
include(avendish.modules)
include(avendish.tools)

include(avendish.ui.qt)
include(avendish.dump)
include(avendish.max)
include(avendish.gstreamer)
include(avendish.pd)
include(avendish.python)
include(avendish.vintage)
include(avendish.vst3)
include(avendish.clap)
include(avendish.ossia)
include(avendish.standalone)
include(avendish.touchdesigner)
include(avendish.godot)
include(avendish.wasm)
include(avendish.fuzz)
include(avendish.example)

# Used for getting completion in IDEs...
function(avnd_register)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "COMPILE_OPTIONS;COMPILE_DEFINITIONS;LINK_LIBRARIES" ${ARGN})

  if(NOT TARGET "${AVND_TARGET}")
    add_library("${AVND_TARGET}" STATIC $<TARGET_OBJECTS:avnd_dummy_lib>)
  endif()

  avnd_add_object_to_base("${AVND_TARGET}" "${AVND_MAIN_FILE}")
  if(AVND_COMPILE_OPTIONS)
    target_compile_options("${AVND_TARGET}" PUBLIC "${AVND_COMPILE_OPTIONS}")
  endif()
  if(AVND_COMPILE_DEFINITIONS)
    target_compile_definitions("${AVND_TARGET}" PUBLIC "${AVND_COMPILE_DEFINITIONS}")
  endif()
  if(AVND_LINK_LIBRARIES)
    target_link_libraries("${AVND_TARGET}" PUBLIC "${AVND_LINK_LIBRARIES}")
  endif()
endfunction()

# Bindings to programming languages, things with a proper object model
function(avnd_make_object)
  avnd_register(${ARGV})

  avnd_make_dump(${ARGV})
  _avnd_dispatch_backend(fuzz ${ARGV})
  avnd_make_ossia(${ARGV})
  _avnd_dispatch_backend(python ${ARGV})
  _avnd_dispatch_backend(pd ${ARGV})
  _avnd_dispatch_backend(max ${ARGV})
  _avnd_dispatch_backend(standalone ${ARGV})
  _avnd_dispatch_backend(wasm ${ARGV})
  avnd_make_example_host(${ARGV})
  _avnd_dispatch_backend(touchdesigner ${ARGV} PROCESSOR_TYPE CHOP_MESSAGE)
  _avnd_dispatch_backend(godot ${ARGV} PROCESSOR_TYPE NODE)
endfunction()

# Bindings to audio plug-in APIs
function(avnd_make_audioplug)
  avnd_register(${ARGV})

  avnd_make_dump(${ARGV})
  _avnd_dispatch_backend(fuzz ${ARGV})
  avnd_make_ossia(${ARGV})
  _avnd_dispatch_backend(vintage ${ARGV})
  _avnd_dispatch_backend(clap ${ARGV})
  _avnd_dispatch_backend(vst3 ${ARGV})
  _avnd_dispatch_backend(wasm ${ARGV})
  avnd_make_example_host(${ARGV})
  _avnd_dispatch_backend(gstreamer ${ARGV} PROCESSOR_TYPE AUDIO)
  _avnd_dispatch_backend(touchdesigner ${ARGV} PROCESSOR_TYPE CHOP_AUDIO)
  _avnd_dispatch_backend(godot ${ARGV} PROCESSOR_TYPE AUDIO_EFFECT)
endfunction()

function(avnd_make_texture)
  avnd_register(${ARGV})

  _avnd_dispatch_backend(fuzz ${ARGV})
  avnd_make_ossia(${ARGV})
  _avnd_dispatch_backend(max ${ARGV})
  _avnd_dispatch_backend(gstreamer ${ARGV} PROCESSOR_TYPE TEXTURE)
  _avnd_dispatch_backend(touchdesigner ${ARGV} PROCESSOR_TYPE TOP)
  _avnd_dispatch_backend(godot ${ARGV} PROCESSOR_TYPE TEXTURE)
endfunction()

function(avnd_make_buffer)
  avnd_register(${ARGV})

  _avnd_dispatch_backend(fuzz ${ARGV})
  _avnd_dispatch_backend(godot ${ARGV} PROCESSOR_TYPE BUFFER)
endfunction()

function(avnd_make_geometry)
  avnd_register(${ARGV})

  _avnd_dispatch_backend(fuzz ${ARGV})
  avnd_make_ossia(${ARGV})
  _avnd_dispatch_backend(max ${ARGV} PROCESSOR_TYPE GEOMETRY)
  _avnd_dispatch_backend(touchdesigner ${ARGV} PROCESSOR_TYPE SOP)
  _avnd_dispatch_backend(touchdesigner ${ARGV} PROCESSOR_TYPE POP)
  _avnd_dispatch_backend(godot ${ARGV} PROCESSOR_TYPE GEOMETRY)
endfunction()

function(avnd_make_all)
  avnd_register(${ARGV})
  avnd_make_ossia(${ARGV})
  avnd_make_example_host(${ARGV})
  avnd_make_object(${ARGV})
  avnd_make_audioplug(${ARGV})
  _avnd_dispatch_backend(gstreamer ${ARGV} PROCESSOR_TYPE AUDIO)
endfunction()

function(avnd_make)
  avnd_register(${ARGV})

  cmake_parse_arguments(AVND "" "" "BACKENDS" ${ARGN})

  foreach(backend ${AVND_BACKENDS})
    string(STRIP "${backend}" backend)
    string(TOLOWER "${backend}" backend)
    _avnd_dispatch_backend(${backend} ${ARGV})
  endforeach()
endfunction()
