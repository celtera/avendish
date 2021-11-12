
avnd_make_all(
  TARGET Minimal
  MAIN_FILE examples/Minimal.hpp
  MAIN_CLASS examples::Minimal
  C_NAME avnd_minimal
)

avnd_make_all(
  TARGET Lowpass
  MAIN_FILE examples/Lowpass.hpp
  MAIN_CLASS examples::Lowpass
  C_NAME avnd_lowpass
)

avnd_make_all(
  TARGET PerSampleProcessor
  MAIN_FILE examples/PerSampleProcessor.hpp
  MAIN_CLASS examples::PerSampleProcessor
  C_NAME avnd_persample_1
)

avnd_make_all(
  TARGET PerSampleProcessor2
  MAIN_FILE examples/PerSampleProcessor2.hpp
  MAIN_CLASS examples::PerSampleProcessor2
  C_NAME avnd_persample_2
)

avnd_make_all(
  TARGET Modular
  MAIN_FILE examples/Modular.hpp
  MAIN_CLASS examples::Modular
  C_NAME avnd_modular
  )

avnd_make_all(
  TARGET Ui
  MAIN_FILE examples/Ui.hpp
  MAIN_CLASS examples::Ui
  C_NAME avnd_ui
  )

avnd_make_all(
  TARGET Presets
  MAIN_FILE examples/Presets.hpp
  MAIN_CLASS examples::Presets
  C_NAME avnd_presets
  )

# This one does not really make sense as a VST
# (it's not an audio processor)
avnd_make_object(
  TARGET Addition
  MAIN_FILE examples/Addition.hpp
  MAIN_CLASS examples::Addition
  C_NAME avnd_addition
)

avnd_make_object(
  TARGET Messages
  MAIN_FILE examples/Messages.hpp
  MAIN_CLASS examples::Messages
  C_NAME avnd_messages
)

avnd_make_object(
  TARGET Init
  MAIN_FILE examples/Init.hpp
  MAIN_CLASS examples::Init
  C_NAME avnd_init
  )

## Neither does this one
avnd_make_object(
  TARGET Callback
  MAIN_FILE examples/Callback.hpp
  MAIN_CLASS examples::Callback
  C_NAME avnd_callback
)

# This one does not really make sense as a Pd or Max object
# (Pd has no notion of MIDI port)
avnd_make_audioplug(
  TARGET Midi
  MAIN_FILE examples/Midi.hpp
  MAIN_CLASS examples::Midi
  C_NAME avnd_midi
)

# GCC segfaults with those two...
if(NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  avnd_make_all(
    TARGET HelpersLogger
    MAIN_FILE examples/Helpers/Logger.hpp
    MAIN_CLASS examples::helpers::Logger
    C_NAME avnd_helpers_logger
    )

  avnd_make_all(
    TARGET HelpersControls
    MAIN_FILE examples/Helpers/Controls.hpp
    MAIN_CLASS examples::helpers::Controls
    C_NAME avnd_helpers_controls
    )
endif()

avnd_make_all(
  TARGET HelpersMessages
  MAIN_FILE examples/Helpers/Messages.hpp
  MAIN_CLASS examples::helpers::Messages
  C_NAME avnd_helpers_messages
  )

avnd_make_all(
  TARGET HelpersPerBusAsArgs
  MAIN_FILE examples/Helpers/PerBus.hpp
  MAIN_CLASS examples::helpers::PerBusAsArgs
  C_NAME avnd_helpers_per_bus_as_args
)

avnd_make_all(
  TARGET HelpersPerBusAsPortsFixed
  MAIN_FILE examples/Helpers/PerBus.hpp
  MAIN_CLASS examples::helpers::PerBusAsPortsFixed
  C_NAME avnd_helpers_per_bus_as_ports_fixed
  )

avnd_make_all(
  TARGET HelpersPerBusAsPortsDynamic
  MAIN_FILE examples/Helpers/PerBus.hpp
  MAIN_CLASS examples::helpers::PerBusAsPortsDynamic
  C_NAME avnd_helpers_per_bus_as_ports_dynamic
  )

avnd_make_all(
  TARGET HelpersPerSampleAsArgs
  MAIN_FILE examples/Helpers/PerSample.hpp
  MAIN_CLASS examples::helpers::PerSampleAsArgs
  C_NAME avnd_helpers_per_sample_as_args
)

avnd_make_all(
  TARGET HelpersPerSampleAsPorts
  MAIN_FILE examples/Helpers/PerSample.hpp
  MAIN_CLASS examples::helpers::PerSampleAsPorts
  C_NAME avnd_helpers_per_sample_as_ports
  )

avnd_make_all(
  TARGET HelpersLowpass
  MAIN_FILE examples/Helpers/Lowpass.hpp
  MAIN_CLASS examples::helpers::Lowpass
  C_NAME avnd_helpers_lowpass
  )

avnd_make_all(
  TARGET HelpersMidi
  MAIN_FILE examples/Helpers/Midi.hpp
  MAIN_CLASS examples::helpers::Midi
  C_NAME avnd_helpers_midi
  )


# Demo: dump all the known metadata.

add_executable(demo_dump examples/Demos/Dump.cpp)
target_link_libraries(demo_dump PRIVATE Avendish)
