include(CMakeFindDependencyMacro)

set(CMAKE_INCLUDE_CURRENT_DIR 1)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(AVND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

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
    "${AVND_SOURCE_DIR}/include/avnd/concepts/fft.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/generic.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/gfx.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/layout.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/message.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/midi.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/midi_port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/modules.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/object.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/painter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/parameter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/processor.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/schedule.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/soundfile.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/concepts/synth.hpp"

    "${AVND_SOURCE_DIR}/include/avnd/introspection/channels.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/input.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/messages.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/midi.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/modules.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/output.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/port.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/introspection/widgets.hpp"

    "${AVND_SOURCE_DIR}/include/avnd/wrappers/audio_channel_manager.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/avnd.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/bus_host_process_adapter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/configure.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/control_display.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls_double.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls_fp.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/controls_storage.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/effect_container.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/metadatas.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/prepare.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_adapter.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/process_execution.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/wrappers/widgets.hpp"

    "${AVND_SOURCE_DIR}/include/avnd/common/arithmetic.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/concepts_polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/coroutines.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/dummy.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/errors.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/enum_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/export.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/for_nth.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/function_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/index_sequence.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/limited_string.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/limited_string_view.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/span_polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/struct_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/widechar.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/init_cpp_with_c_object_header.hpp"

    "${AVND_SOURCE_DIR}/include/halp/attributes.hpp"
    "${AVND_SOURCE_DIR}/include/halp/audio.hpp"
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
    "${AVND_SOURCE_DIR}/include/halp/polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/halp/reactive_value.hpp"
    "${AVND_SOURCE_DIR}/include/halp/sample_accurate_controls.hpp"
    "${AVND_SOURCE_DIR}/include/halp/schedule.hpp"
    "${AVND_SOURCE_DIR}/include/halp/smoothers.hpp"
    "${AVND_SOURCE_DIR}/include/halp/soundfile_port.hpp"
    "${AVND_SOURCE_DIR}/include/halp/static_string.hpp"
    "${AVND_SOURCE_DIR}/include/halp/texture.hpp"
    "${AVND_SOURCE_DIR}/include/halp/texture_formats.hpp"
    "${AVND_SOURCE_DIR}/include/halp/value_types.hpp"

    "${AVND_SOURCE_DIR}/include/gpp/commands.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/generators.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/meta.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/layout.hpp"
)

add_library(avnd_dummy_lib OBJECT "${AVND_SOURCE_DIR}/src/dummy.cpp")

include(avendish.dependencies)
include(avendish.disableexceptions)
include(avendish.sources)
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
include(avendish.example)

# Used for getting completion in IDEs...
function(avnd_register)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "COMPILE_OPTIONS;COMPILE_DEFINITIONS;LINK_LIBRARIES" ${ARGN})

  if(NOT TARGET "${AVND_TARGET}")
    add_library("${AVND_TARGET}" STATIC $<TARGET_OBJECTS:avnd_dummy_lib>)
  endif()

  target_sources("${AVND_TARGET}" PRIVATE "${AVND_MAIN_FILE}")
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
  avnd_make_ossia(${ARGV})
  avnd_make_python(${ARGV})
  avnd_make_pd(${ARGV})
  avnd_make_max(${ARGV})
  avnd_make_standalone(${ARGV})
  avnd_make_example_host(${ARGV})
endfunction()

# Bindings to audio plug-in APIs
function(avnd_make_audioplug)
  avnd_register(${ARGV})

  avnd_make_dump(${ARGV})
  avnd_make_ossia(${ARGV})
  avnd_make_vintage(${ARGV})
  avnd_make_clap(${ARGV})
  avnd_make_vst3(${ARGV})
  avnd_make_example_host(${ARGV})
endfunction()

function(avnd_make_all)
  avnd_register(${ARGV})
  avnd_make_ossia(${ARGV})
  avnd_make_example_host(${ARGV})
  avnd_make_object(${ARGV})
  avnd_make_audioplug(${ARGV})
  avnd_make_gstreamer(${ARGV})
endfunction()

function(avnd_make)
  avnd_register(${ARGV})

  cmake_parse_arguments(AVND "" "" "BACKENDS" ${ARGN})

  foreach(backend ${AVND_BACKENDS})
    string(STRIP "${backend}" backend)
    string(TOLOWER "${backend}" backend)
    cmake_language(CALL "avnd_make_${backend}" ${ARGV})
  endforeach()
endfunction()
