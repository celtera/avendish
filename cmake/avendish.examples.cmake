
avnd_make_object(
  TARGET Construct
  MAIN_FILE examples/Raw/Construct.hpp
  MAIN_CLASS examples::Construct
  C_NAME avnd_construct
)

avnd_make_all(
  TARGET Minimal
  MAIN_FILE examples/Raw/Minimal.hpp
  MAIN_CLASS examples::Minimal
  C_NAME avnd_minimal
)

avnd_make_all(
  TARGET Lowpass
  MAIN_FILE examples/Raw/Lowpass.hpp
  MAIN_CLASS examples::Lowpass
  C_NAME avnd_lowpass
)

avnd_make_all(
  TARGET PerSampleProcessor
  MAIN_FILE examples/Raw/PerSampleProcessor.hpp
  MAIN_CLASS examples::PerSampleProcessor
  C_NAME avnd_persample_1
)

avnd_make_all(
  TARGET PerSampleProcessor2
  MAIN_FILE examples/Raw/PerSampleProcessor2.hpp
  MAIN_CLASS examples::PerSampleProcessor2
  C_NAME avnd_persample_2
)

avnd_make_all(
  TARGET Modular
  MAIN_FILE examples/Raw/Modular.hpp
  MAIN_CLASS examples::Modular
  C_NAME avnd_modular
  )

avnd_make_all(
  TARGET Ui
  MAIN_FILE examples/Raw/Ui.hpp
  MAIN_CLASS examples::Ui
  C_NAME avnd_ui
  )

avnd_make_all(
  TARGET Presets
  MAIN_FILE examples/Raw/Presets.hpp
  MAIN_CLASS examples::Presets
  C_NAME avnd_presets
  )

# These really make sense as a VST
# (it's not an audio processor)
avnd_make_object(
  TARGET Addition
  MAIN_FILE examples/Raw/Addition.hpp
  MAIN_CLASS examples::Addition
  C_NAME avnd_addition
)

avnd_make_object(
  TARGET Random
  MAIN_FILE examples/Raw/Random.hpp
  MAIN_CLASS examples::Random
  C_NAME avnd_random
)

avnd_make_object(
  TARGET Messages
  MAIN_FILE examples/Raw/Messages.hpp
  MAIN_CLASS examples::Messages
  C_NAME avnd_messages
)

avnd_make_object(
  TARGET Init
  MAIN_FILE examples/Raw/Init.hpp
  MAIN_CLASS examples::Init
  C_NAME avnd_init
  )

avnd_make_all(
  TARGET Sines
  MAIN_FILE examples/Raw/Sines.hpp
  MAIN_CLASS examples::Sine
  C_NAME avnd_sines
)

avnd_make_object(
  TARGET Callback
  MAIN_FILE examples/Raw/Callback.hpp
  MAIN_CLASS examples::Callback
  C_NAME avnd_callback
)

avnd_make_object(
  TARGET Peak
  MAIN_FILE examples/Helpers/Peak.hpp
  MAIN_CLASS examples::helpers::Peak
  C_NAME avnd_peak
)


avnd_make_object(
  TARGET CompleteMessageExample
  MAIN_FILE examples/Complete/CompleteMessageExample.hpp
  MAIN_CLASS examples::helpers::CompleteMessageExample
  C_NAME avnd_complete_message_example
)



# This one does not really make sense as a Pd or Max object
# (Pd has no notion of MIDI port)
avnd_make_audioplug(
  TARGET Midi
  MAIN_FILE examples/Raw/Midi.hpp
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
    TARGET HelpersUi
    MAIN_FILE examples/Helpers/Ui.hpp
    MAIN_CLASS examples::helpers::Ui
    C_NAME avnd_helpers_ui
    )
endif()

avnd_make_all(
  TARGET HelpersControls
  MAIN_FILE examples/Helpers/Controls.hpp
  MAIN_CLASS examples::helpers::Controls
  C_NAME avnd_helpers_controls
  )

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

avnd_make_all(
  TARGET SampleAccurateControls
  MAIN_FILE examples/Raw/SampleAccurateControls.hpp
  MAIN_CLASS examples::SampleAccurateControls
  C_NAME avnd_sample_accurate_controls
)

avnd_make_all(
  TARGET WhiteNoise
  MAIN_FILE examples/Helpers/Noise.hpp
  MAIN_CLASS examples::helpers::WhiteNoise
  C_NAME avnd_white_noise
)


set(OSCR_EXAMPLES
  AudioEffectExample
  AudioSidechainExample
  Distortion
  EmptyExample
  SampleAccurateFilterExample
  SampleAccurateGeneratorExample
  TrivialFilterExample
  TrivialGeneratorExample
  ZeroDependencyAudioEffect
)
foreach(theTarget ${OSCR_EXAMPLES})
  avnd_make_all(
    TARGET Tutorial_${theTarget}
    MAIN_FILE examples/Tutorial/${theTarget}.hpp
    MAIN_CLASS examples::${theTarget}
    C_NAME oscr_${theTarget}
  )
endforeach()

# This one needs libossia
set(OSSIA_EXAMPLES
  ControlGallery
  Synth
  TextureFilterExample
  TextureGeneratorExample
)
foreach(theTarget ${OSSIA_EXAMPLES})
  avnd_make_ossia(
    TARGET Tutorial_${theTarget}
    MAIN_FILE examples/Tutorial/${theTarget}.hpp
    MAIN_CLASS examples::${theTarget}
    C_NAME oscr_${theTarget}
  )
endforeach()

###### Ports

## Essentia
avnd_make_all(
  TARGET Essentia_Entropy
  MAIN_FILE examples/Ports/Essentia/stats/Entropy.hpp
  MAIN_CLASS essentia_ports::Entropy
  C_NAME avnd_essentia_entropy
)

## LitterPower
avnd_make_object(
  TARGET CCC
  MAIN_FILE examples/Ports/LitterPower/CCC.hpp
  MAIN_CLASS litterpower_ports::CCC
  C_NAME avnd_lp_ccc
)

## vb-objects
avnd_make_object(
  TARGET VB_fourses_tilde
  MAIN_FILE examples/Ports/VB/vb.fourses_tilde.hpp
  MAIN_CLASS vb_ports::fourses_tilde
  C_NAME avnd_vb_fourses_tilde
)

## WaveDigitalFilters
# find_path (CHOWDSP_WDF_INCLUDE chowdsp_wdf/chowdsp_wdf.h)
# if(CHOWDSP_WDF_INCLUDE)
#   add_library(wdft_PassiveLPF STATIC src/dummy.cpp)
#   target_include_directories(wdft_PassiveLPF PUBLIC ${CHOWDSP_WDF_INCLUDE})
#   avnd_make_all(
#     TARGET wdft_PassiveLPF
#     MAIN_FILE examples/Ports/WaveDigitalFilters/PassiveLPF.hpp
#     MAIN_CLASS chowdsp_ports::PassiveLPF
#     C_NAME wdft_PassiveLPF
#   )
#
#   add_library(wdft_VoltageDivider STATIC src/dummy.cpp)
#   target_include_directories(wdft_VoltageDivider PUBLIC ${CHOWDSP_WDF_INCLUDE})
#   avnd_make_all(
#     TARGET wdft_VoltageDivider
#     MAIN_FILE examples/Ports/WaveDigitalFilters/VoltageDivider.hpp
#     MAIN_CLASS chowdsp_ports::VoltageDivider
#     C_NAME wdft_VoltageDivider
#   )
# endif()


find_library(liblo NAMES lo)
if(liblo)
add_executable(demo_osc examples/Demos/OSC.cpp)
target_link_libraries(demo_osc PRIVATE Avendish ${liblo})
endif()

