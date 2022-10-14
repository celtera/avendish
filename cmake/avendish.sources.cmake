add_library(Avendish)
add_library(Avendish::Avendish ALIAS Avendish)

target_compile_definitions(Avendish PUBLIC -DFMT_HEADER_ONLY=1)

if(MSVC)
  target_compile_options(Avendish PUBLIC /std:c++latest)
  target_compile_definitions(Avendish PUBLIC -DNOMINMAX=1 -DWIN32_LEAN_AND_MEAN=1)
endif()

target_sources(Avendish
  PUBLIC
    "${AVND_SOURCE_DIR}/include/avnd/concepts/all.hpp"
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

    "${AVND_SOURCE_DIR}/include/avnd/common/aggregates.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/concepts_polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/coroutines.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/dummy.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/errors.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/export.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/for_nth.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/function_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/index_sequence.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/limited_string.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/limited_string_view.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/span_polyfill.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/struct_reflection.hpp"
    "${AVND_SOURCE_DIR}/include/avnd/common/widechar.hpp"

    "${AVND_SOURCE_DIR}/include/halp/audio.hpp"
    "${AVND_SOURCE_DIR}/include/halp/callback.hpp"
    "${AVND_SOURCE_DIR}/include/halp/controls.hpp"
    "${AVND_SOURCE_DIR}/include/halp/layout.hpp"
    "${AVND_SOURCE_DIR}/include/halp/log.hpp"
    "${AVND_SOURCE_DIR}/include/halp/messages.hpp"
    "${AVND_SOURCE_DIR}/include/halp/meta.hpp"
    "${AVND_SOURCE_DIR}/include/halp/midi.hpp"
    "${AVND_SOURCE_DIR}/include/halp/reactive_value.hpp"
    "${AVND_SOURCE_DIR}/include/halp/sample_accurate_controls.hpp"
    "${AVND_SOURCE_DIR}/include/halp/static_string.hpp"
    "${AVND_SOURCE_DIR}/include/halp/texture.hpp"

    "${AVND_SOURCE_DIR}/include/gpp/commands.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/generators.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/meta.hpp"
    "${AVND_SOURCE_DIR}/include/gpp/layout.hpp"

    "${AVND_SOURCE_DIR}/src/dummy.cpp"
)

if(AVENDISH_INCLUDE_SOURCE_ONLY)
  target_include_directories(
      Avendish
      PUBLIC
        $<BUILD_INTERFACE:${AVND_SOURCE_DIR}/include>
  )
  return()
endif()

function(avnd_target_setup AVND_FX_TARGET)
  target_compile_features(
      ${AVND_FX_TARGET}
      PUBLIC
        cxx_std_20
  )

  if(UNIX AND NOT APPLE)
    target_compile_options(
      ${AVND_FX_TARGET}
      PUBLIC
        -fno-semantic-interposition
        -fPIC
    )
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_compile_options(
        ${AVND_FX_TARGET}
        PUBLIC
          -fcoroutines
          # -flto
          -ffunction-sections
          -fdata-sections
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
    target_compile_options(
        ${AVND_FX_TARGET}
        PUBLIC
          -fcoroutines-ts
          -stdlib=libc++
          # -flto
          -fno-stack-protector
          -fno-ident
          -fno-plt
          -ffunction-sections
          -fdata-sections
    )

    if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER_EQUAL 13.0)
      # target_compile_options(
      #   ${AVND_FX_TARGET}
      #   PUBLIC
      #     -fvisibility-inlines-hidden-static-local-var
      #     -fvirtual-function-elimination
      # )
    endif()
  endif()

  if(TARGET fmt::fmt)
    target_link_libraries(
        ${AVND_FX_TARGET}
        PUBLIC fmt::fmt
    )
  elseif(TARGET fmt::fmt_header_only)
    target_link_libraries(
        ${AVND_FX_TARGET}
        PUBLIC fmt::fmt_header_only
    )
  endif()

  target_include_directories(
      ${AVND_FX_TARGET}
      PUBLIC
        $<BUILD_INTERFACE:${AVND_SOURCE_DIR}/include>
  )

  if(UNIX AND NOT APPLE)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE
   #   -static-libgcc
   #   -static-libstdc++
      -Wl,--gc-sections
    )
  endif()

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_link_libraries(${AVND_FX_TARGET}
      PRIVATE
        -Bsymbolic
        # -flto
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    target_link_libraries(${AVND_FX_TARGET} PRIVATE
      -lc++
      -Bsymbolic
      # -flto
    )
  endif()

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      PREFIX ""
      POSITION_INDEPENDENT_CODE 1
      VISIBILITY_INLINES_HIDDEN 1
      CXX_VISIBILITY_PRESET internal
  )

  target_link_libraries(${AVND_FX_TARGET} PUBLIC Boost::boost)
endfunction()

function(avnd_common_setup AVND_TARGET AVND_FX_TARGET)
  avnd_target_setup("${AVND_FX_TARGET}")

  if(TARGET "${AVND_TARGET}")
    avnd_target_setup("${AVND_TARGET}")

    target_link_libraries(
      ${AVND_FX_TARGET}
      PUBLIC
        ${AVND_TARGET}
    )
  endif()
endfunction()

avnd_common_setup("" "Avendish")
