
add_library(Avendish)
add_library(Avendish::Avendish ALIAS Avendish)

target_sources(Avendish
  PRIVATE
    src/avnd/concepts/audio_port.hpp
    src/avnd/concepts/audio_processor.hpp
    src/avnd/concepts/callback.hpp
    src/avnd/concepts/channels.hpp
    src/avnd/concepts/generic.hpp
    src/avnd/concepts/message.hpp
    src/avnd/concepts/midi.hpp
    src/avnd/concepts/midi_port.hpp
    src/avnd/concepts/modules.hpp
    src/avnd/concepts/object.hpp
    src/avnd/concepts/parameter.hpp
    src/avnd/concepts/port.hpp
    src/avnd/concepts/processor.hpp
    src/avnd/concepts/synth.hpp

    src/avnd/avnd.hpp
    src/avnd/channels_introspection.hpp
    src/avnd/concepts.hpp
    src/avnd/configure.hpp
    src/avnd/controls.hpp
    src/avnd/effect_container.hpp
    src/avnd/port_introspection.hpp
    src/avnd/input_introspection.hpp
    src/avnd/messages_introspection.hpp
    src/avnd/midi_introspection.hpp
    src/avnd/modules_introspection.hpp
    src/avnd/output_introspection.hpp
    src/avnd/prepare.hpp
    src/avnd/process_adapter.hpp

    src/common/coroutines.hpp
    src/common/concepts_polyfill.hpp
    src/common/export.hpp
    src/common/for_nth.hpp
    src/common/function_reflection.hpp
    src/common/index_sequence.hpp
    src/common/limited_string.hpp
    src/common/limited_string_view.hpp
    src/common/struct_reflection.hpp

    src/pd/atom_iterator.hpp
    src/pd/audio_processor.hpp
    src/pd/configure.hpp
    src/pd/dsp.hpp
    src/pd/init.hpp
    src/pd/inputs.hpp
    src/pd/message_processor.hpp
    src/pd/messages.hpp
    src/pd/outputs.hpp
    src/pd/helpers.hpp

    src/python/processor.hpp
    src/python/configure.hpp

    src/max/atom_iterator.hpp
    src/max/audio_processor.hpp
    src/max/configure.hpp
    src/max/dsp.hpp
    src/max/init.hpp
    src/max/message_processor.hpp
    src/max/inputs.hpp
    src/max/messages.hpp
    src/max/outputs.hpp
    src/max/helpers.hpp

    src/standalone/audio.hpp
    src/standalone/configure.hpp
    src/standalone/standalone.hpp
    src/standalone/oscquery_mapper.hpp

    src/vintage/audio_effect.hpp
    src/vintage/atomic_controls.hpp
    src/vintage/configure.hpp
    src/vintage/dispatch.hpp
    src/vintage/helpers.hpp
    src/vintage/midi_processor.hpp
    src/vintage/processor_setup.hpp
    src/vintage/programs.hpp
    src/vintage/vintage.hpp

    src/helpers/callback.hpp
    src/helpers/reactive_value.hpp
    src/helpers/controls.hpp
    src/helpers/log.hpp

    src/ui/qml_ui.hpp
    src/ui/qml/enum_control.hpp
    src/ui/qml/enum_ui.hpp
    src/ui/qml/toggle_control.hpp
    src/ui/qml/toggle_ui.hpp
    src/ui/qml/float_control.hpp
    src/ui/qml/float_knob.hpp
    src/ui/qml/float_slider.hpp
    src/ui/qml/int_control.hpp
    src/ui/qml/int_knob.hpp
    src/ui/qml/int_slider.hpp

    src/dummy.cpp
)

function(avnd_common_setup AVND_TARGET AVND_FX_TARGET)
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
    )
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_compile_options(
        ${AVND_FX_TARGET}
        PUBLIC
          -fcoroutines
          -flto
          -ffunction-sections
          -fdata-sections
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
    target_compile_options(
        ${AVND_FX_TARGET}
        PUBLIC
          -fcoroutines-ts
          -stdlib=libc++
          -flto
          -fvisibility-inlines-hidden-static-local-var
          -fno-stack-protector
          -fno-ident
          -fno-plt
          -fvirtual-function-elimination
          -ffunction-sections
          -fdata-sections
    )
  endif()

  target_compile_definitions(
      ${AVND_FX_TARGET}
      PUBLIC
        FMT_HEADER_ONLY=1
  )

  target_include_directories(
      ${AVND_FX_TARGET}
      PUBLIC
        src
  )

  if(UNIX AND NOT APPLE)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE -static-libgcc -static-libstdc++ -Wl,--gc-sections)
  endif()

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_link_libraries(${AVND_FX_TARGET}
      PRIVATE
        -Bsymbolic
        -flto
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    target_link_libraries(${AVND_FX_TARGET} PRIVATE
      -lc++
      -Bsymbolic
      -flto
    )
  endif()

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      PREFIX ""
      POSITION_INDEPENDENT_CODE 1
      VISIBILITY_INLINES_HIDDEN 1
      # CXX_VISIBILITY_PRESET internal
  )

  target_link_libraries(${AVND_FX_TARGET} PUBLIC Boost::boost)

  if(TARGET "${AVND_TARGET}")
    target_link_libraries(
      ${AVND_FX_TARGET}
      PUBLIC
        ${AVND_TARGET}
    )
  endif()
endfunction()

avnd_common_setup("" "Avendish")
