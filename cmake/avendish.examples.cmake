
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

if(NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  avnd_make_object(
    TARGET Helpers
    MAIN_FILE examples/Helpers.hpp
    MAIN_CLASS examples::Helpers
    C_NAME avnd_helpers
    )
endif()

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
avnd_make_vintage(
  TARGET Midi
  MAIN_FILE examples/Midi.hpp
  MAIN_CLASS examples::Midi
  C_NAME avnd_midi
)
