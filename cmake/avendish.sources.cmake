add_library(Avendish)
add_library(Avendish::Avendish ALIAS Avendish)

target_sources(Avendish
  PRIVATE
    include/avnd/concepts/audio_port.hpp
    include/avnd/concepts/audio_processor.hpp
    include/avnd/concepts/callback.hpp
    include/avnd/concepts/channels.hpp
    include/avnd/concepts/generic.hpp
    include/avnd/concepts/message.hpp
    include/avnd/concepts/midi.hpp
    include/avnd/concepts/midi_port.hpp
    include/avnd/concepts/modules.hpp
    include/avnd/concepts/object.hpp
    include/avnd/concepts/parameter.hpp
    include/avnd/concepts/port.hpp
    include/avnd/concepts/processor.hpp
    include/avnd/concepts/synth.hpp

    include/avnd/wrappers/avnd.hpp
    include/avnd/wrappers/channels_introspection.hpp
    include/avnd/wrappers/concepts.hpp
    include/avnd/wrappers/configure.hpp
    include/avnd/wrappers/controls.hpp
    include/avnd/wrappers/controls_fp.hpp
    include/avnd/wrappers/control_display.hpp
    include/avnd/wrappers/effect_container.hpp
    include/avnd/wrappers/port_introspection.hpp
    include/avnd/wrappers/input_introspection.hpp
    include/avnd/wrappers/messages_introspection.hpp
    include/avnd/wrappers/midi_introspection.hpp
    include/avnd/wrappers/modules_introspection.hpp
    include/avnd/wrappers/output_introspection.hpp
    include/avnd/wrappers/prepare.hpp
    include/avnd/wrappers/process_adapter.hpp

    include/avnd/common/coroutines.hpp
    include/avnd/common/concepts_polyfill.hpp
    include/avnd/common/export.hpp
    include/avnd/common/for_nth.hpp
    include/avnd/common/function_reflection.hpp
    include/avnd/common/index_sequence.hpp
    include/avnd/common/limited_string.hpp
    include/avnd/common/limited_string_view.hpp
    include/avnd/common/struct_reflection.hpp
    include/avnd/common/widechar.hpp

    include/avnd/helpers/audio.hpp
    include/avnd/helpers/callback.hpp
    include/avnd/helpers/reactive_value.hpp
    include/avnd/helpers/controls.hpp
    include/avnd/helpers/log.hpp
    include/avnd/helpers/meta.hpp
    include/avnd/helpers/static_string.hpp

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

  target_compile_definitions(
      ${AVND_FX_TARGET}
      PUBLIC
        FMT_HEADER_ONLY=1
  )

  target_include_directories(
      ${AVND_FX_TARGET}
      PUBLIC
        include
  )

  if(UNIX AND NOT APPLE)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE
      -static-libgcc
      -static-libstdc++
      -Wl,--gc-sections
    )
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
