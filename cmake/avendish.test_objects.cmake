# Test-OBJECT battery (distinct from avendish.tests.cmake, which is the Catch2/CTest
# unit tests). See AVND_FEATURE_CATALOG.md / AVND_TEST_HARNESS_PLAN.md.
#
# One isolated plugin object PER FILE so a failure on a given backend points at
# exactly one thing. Texture objects use avnd_make_texture (max/jitter, td:TOP,
# godot:TEXTURE, gstreamer:TEXTURE). Value/control/audio/midi objects use
# avnd_make_all (max, pd, td:CHOP_MESSAGE, godot:NODE, python, wasm, standalone,
# ossia, vst3/clap, gstreamer). Every object also gets a libFuzzer target (when
# AVND_ENABLE_FUZZ=ON) + a dump JSON (the contract the patch generators consume).

### Texture (§5) ###
avnd_make_texture(TARGET TestTexRGBA8   MAIN_FILE examples/Tests/TestTexRGBA8Passthrough.hpp   MAIN_CLASS examples::tests::TestTexRGBA8Passthrough   C_NAME avnd_test_tex_rgba8)
avnd_make_texture(TARGET TestTexRGB     MAIN_FILE examples/Tests/TestTexRGBPassthrough.hpp     MAIN_CLASS examples::tests::TestTexRGBPassthrough     C_NAME avnd_test_tex_rgb8)
avnd_make_texture(TARGET TestTexR32F    MAIN_FILE examples/Tests/TestTexR32FPassthrough.hpp    MAIN_CLASS examples::tests::TestTexR32FPassthrough    C_NAME avnd_test_tex_r32f)
avnd_make_texture(TARGET TestTexRGBA32F MAIN_FILE examples/Tests/TestTexRGBA32FPassthrough.hpp MAIN_CLASS examples::tests::TestTexRGBA32FPassthrough C_NAME avnd_test_tex_rgba32f)
avnd_make_texture(TARGET TestTexGen     MAIN_FILE examples/Tests/TestTexGenerator.hpp          MAIN_CLASS examples::tests::TestTexGenerator          C_NAME avnd_test_tex_generator)
avnd_make_texture(TARGET TestTexVar     MAIN_FILE examples/Tests/TestTexVariableInput.hpp      MAIN_CLASS examples::tests::TestTexVariableInput      C_NAME avnd_test_tex_variable)

### Controls / value I/O (§1, §2) ###
avnd_make_all(TARGET TestFloatSlider      MAIN_FILE examples/Tests/TestFloatSlider.hpp      MAIN_CLASS examples::tests::TestFloatSlider       C_NAME avnd_test_float_slider)
avnd_make_all(TARGET TestFloatKnob        MAIN_FILE examples/Tests/TestFloatKnob.hpp        MAIN_CLASS examples::tests::TestFloatKnob         C_NAME avnd_test_float_knob)
avnd_make_all(TARGET TestIntSlider        MAIN_FILE examples/Tests/TestIntSlider.hpp        MAIN_CLASS examples::tests::TestIntSlider         C_NAME avnd_test_int_slider)
avnd_make_all(TARGET TestToggle           MAIN_FILE examples/Tests/TestToggle.hpp           MAIN_CLASS examples::tests::TestToggle            C_NAME avnd_test_toggle)
avnd_make_all(TARGET TestLineEdit         MAIN_FILE examples/Tests/TestLineEdit.hpp         MAIN_CLASS examples::tests::TestLineEdit          C_NAME avnd_test_lineedit)
avnd_make_all(TARGET TestImpulseButton    MAIN_FILE examples/Tests/TestImpulseButton.hpp    MAIN_CLASS examples::tests::TestImpulseButton     C_NAME avnd_test_impulse)
avnd_make_all(TARGET TestMaintainedButton MAIN_FILE examples/Tests/TestMaintainedButton.hpp MAIN_CLASS examples::tests::TestMaintainedButton  C_NAME avnd_test_maintained)
avnd_make_all(TARGET TestEnum             MAIN_FILE examples/Tests/TestEnum.hpp             MAIN_CLASS examples::tests::TestEnum              C_NAME avnd_test_enum)
avnd_make_all(TARGET TestXYPad            MAIN_FILE examples/Tests/TestXYPad.hpp            MAIN_CLASS examples::tests::TestXYPad             C_NAME avnd_test_xy)
avnd_make_all(TARGET TestBargraphOutput   MAIN_FILE examples/Tests/TestBargraphOutput.hpp   MAIN_CLASS examples::tests::TestBargraphOutput    C_NAME avnd_test_bargraph)

### Values / messages / callbacks (§2, §9) ###
avnd_make_all(TARGET TestValueFloatIO  MAIN_FILE examples/Tests/TestValueFloatIO.hpp  MAIN_CLASS examples::tests::TestValueFloatIO  C_NAME avnd_test_val_float)
avnd_make_all(TARGET TestValueIntIO    MAIN_FILE examples/Tests/TestValueIntIO.hpp    MAIN_CLASS examples::tests::TestValueIntIO    C_NAME avnd_test_val_int)
avnd_make_all(TARGET TestValueBoolIO   MAIN_FILE examples/Tests/TestValueBoolIO.hpp   MAIN_CLASS examples::tests::TestValueBoolIO   C_NAME avnd_test_val_bool)
avnd_make_all(TARGET TestValueStringIO MAIN_FILE examples/Tests/TestValueStringIO.hpp MAIN_CLASS examples::tests::TestValueStringIO C_NAME avnd_test_val_string)
avnd_make_all(TARGET TestMessages      MAIN_FILE examples/Tests/TestMessages.hpp      MAIN_CLASS examples::tests::TestMessages      C_NAME avnd_test_messages)
avnd_make_all(TARGET TestCallback      MAIN_FILE examples/Tests/TestCallback.hpp      MAIN_CLASS examples::tests::TestCallback      C_NAME avnd_test_callback)

### MIDI (§4) ###
avnd_make_all(TARGET TestMidiPassthrough MAIN_FILE examples/Tests/TestMidiPassthrough.hpp MAIN_CLASS examples::tests::TestMidiPassthrough C_NAME avnd_test_midi)

### Audio (§3) ###
avnd_make_all(TARGET TestAudioGainMono MAIN_FILE examples/Tests/TestAudioGainMono.hpp MAIN_CLASS examples::tests::TestAudioGainMono C_NAME avnd_test_audio_mono)
avnd_make_all(TARGET TestAudioGainPoly MAIN_FILE examples/Tests/TestAudioGainPoly.hpp MAIN_CLASS examples::tests::TestAudioGainPoly C_NAME avnd_test_audio_poly)

### Buffer / Tensor (§6, §7) ###
avnd_make_all(TARGET TestBufferRawIO   MAIN_FILE examples/Tests/TestBufferRawIO.hpp   MAIN_CLASS examples::tests::TestBufferRawIO   C_NAME avnd_test_buffer_raw)
avnd_make(TARGET TestBufferTypedIO MAIN_FILE examples/Tests/TestBufferTypedIO.hpp MAIN_CLASS examples::tests::TestBufferTypedIO C_NAME avnd_test_buffer_typed BACKENDS dump ossia pd max)
avnd_make_all(TARGET TestTensorInput   MAIN_FILE examples/Tests/TestTensorInput.hpp   MAIN_CLASS examples::tests::TestTensorInput   C_NAME avnd_test_tensor)

### Metadata (§11) ###
avnd_make_all(TARGET TestMetadata      MAIN_FILE examples/Tests/TestMetadata.hpp      MAIN_CLASS examples::tests::TestMetadata      C_NAME avnd_test_metadata)

### More controls + structured (§1, §10) ###
avnd_make_all(TARGET TestSpinbox   MAIN_FILE examples/Tests/TestSpinbox.hpp   MAIN_CLASS examples::tests::TestSpinbox   C_NAME avnd_test_spinbox)
avnd_make_all(TARGET TestColor     MAIN_FILE examples/Tests/TestColor.hpp     MAIN_CLASS examples::tests::TestColor     C_NAME avnd_test_color)
avnd_make(TARGET TestAggregate MAIN_FILE examples/Tests/TestAggregate.hpp MAIN_CLASS examples::tests::TestAggregate C_NAME avnd_test_aggregate BACKENDS dump ossia pd max)

### Audio forms (§3) ###
avnd_make_all(TARGET TestAudioSamplePorts     MAIN_FILE examples/Tests/TestAudioSamplePorts.hpp     MAIN_CLASS examples::tests::TestAudioSamplePorts     C_NAME avnd_test_audio_sample_ports)
avnd_make_all(TARGET TestAudioBusArgs         MAIN_FILE examples/Tests/TestAudioBusArgs.hpp         MAIN_CLASS examples::tests::TestAudioBusArgs         C_NAME avnd_test_audio_bus_args)
avnd_make_all(TARGET TestAudioBusFixed        MAIN_FILE examples/Tests/TestAudioBusFixed.hpp        MAIN_CLASS examples::tests::TestAudioBusFixed        C_NAME avnd_test_audio_bus_fixed)
avnd_make_all(TARGET TestAudioFrame           MAIN_FILE examples/Tests/TestAudioFrame.hpp           MAIN_CLASS examples::tests::TestAudioFrame           C_NAME avnd_test_audio_frame)
avnd_make_all(TARGET TestAudioVariableChannels MAIN_FILE examples/Tests/TestAudioVariableChannels.hpp MAIN_CLASS examples::tests::TestAudioVariableChannels C_NAME avnd_test_audio_variable)

### GPU buffers (§6) ###
avnd_make(TARGET TestGpuBufferInput  MAIN_FILE examples/Tests/TestGpuBufferInput.hpp  MAIN_CLASS examples::tests::TestGpuBufferInput  C_NAME avnd_test_gpu_buffer_in BACKENDS dump ossia pd)
avnd_make(TARGET TestGpuBufferOutput MAIN_FILE examples/Tests/TestGpuBufferOutput.hpp MAIN_CLASS examples::tests::TestGpuBufferOutput C_NAME avnd_test_gpu_buffer_out BACKENDS dump ossia pd)

### Geometry: static / dynamic, CPU / GPU (§8) ###
avnd_make_geometry(TARGET TestGeometryStaticGenerator MAIN_FILE examples/Tests/TestGeometryStaticGenerator.hpp MAIN_CLASS examples::tests::TestGeometryStaticGenerator C_NAME avnd_test_geom_static)
avnd_make_geometry(TARGET TestGeometryDynamicFilter   MAIN_FILE examples/Tests/TestGeometryDynamicFilter.hpp   MAIN_CLASS examples::tests::TestGeometryDynamicFilter   C_NAME avnd_test_geom_dyn)
avnd_make_geometry(TARGET TestGpuGeometryFilter       MAIN_FILE examples/Tests/TestGpuGeometryFilter.hpp       MAIN_CLASS examples::tests::TestGpuGeometryFilter       C_NAME avnd_test_geom_gpu)

### Ticks / lifecycle (§12) ###
avnd_make_all(TARGET TestTick         MAIN_FILE examples/Tests/TestTick.hpp         MAIN_CLASS examples::tests::TestTick         C_NAME avnd_test_tick)
avnd_make_all(TARGET TestTickMusical  MAIN_FILE examples/Tests/TestTickMusical.hpp  MAIN_CLASS examples::tests::TestTickMusical  C_NAME avnd_test_tick_musical)
avnd_make_all(TARGET TestTickFlicks   MAIN_FILE examples/Tests/TestTickFlicks.hpp   MAIN_CLASS examples::tests::TestTickFlicks   C_NAME avnd_test_tick_flicks)
avnd_make_all(TARGET TestPrepare      MAIN_FILE examples/Tests/TestPrepare.hpp      MAIN_CLASS examples::tests::TestPrepare      C_NAME avnd_test_prepare)
avnd_make_all(TARGET TestInitialize   MAIN_FILE examples/Tests/TestInitialize.hpp   MAIN_CLASS examples::tests::TestInitialize   C_NAME avnd_test_initialize)
avnd_make_all(TARGET TestBypass       MAIN_FILE examples/Tests/TestBypass.hpp       MAIN_CLASS examples::tests::TestBypass       C_NAME avnd_test_bypass)

### More controls (§1) ###
avnd_make_all(TARGET TestRangeSlider   MAIN_FILE examples/Tests/TestRangeSlider.hpp   MAIN_CLASS examples::tests::TestRangeSlider   C_NAME avnd_test_range_slider)
avnd_make_all(TARGET TestXYZSpinboxes  MAIN_FILE examples/Tests/TestXYZSpinboxes.hpp  MAIN_CLASS examples::tests::TestXYZSpinboxes  C_NAME avnd_test_xyz)
avnd_make_all(TARGET TestXYZWSpinboxes MAIN_FILE examples/Tests/TestXYZWSpinboxes.hpp MAIN_CLASS examples::tests::TestXYZWSpinboxes C_NAME avnd_test_xyzw)
avnd_make_all(TARGET TestTimeChooser   MAIN_FILE examples/Tests/TestTimeChooser.hpp   MAIN_CLASS examples::tests::TestTimeChooser   C_NAME avnd_test_time)
avnd_make_all(TARGET TestStringEnum    MAIN_FILE examples/Tests/TestStringEnum.hpp    MAIN_CLASS examples::tests::TestStringEnum    C_NAME avnd_test_string_enum)
avnd_make_all(TARGET TestFolderPort    MAIN_FILE examples/Tests/TestFolderPort.hpp    MAIN_CLASS examples::tests::TestFolderPort    C_NAME avnd_test_folder)
avnd_make_all(TARGET TestFilePort      MAIN_FILE examples/Tests/TestFilePort.hpp      MAIN_CLASS examples::tests::TestFilePort      C_NAME avnd_test_file)
avnd_make_all(TARGET TestMultiSlider   MAIN_FILE examples/Tests/TestMultiSlider.hpp   MAIN_CLASS examples::tests::TestMultiSlider   C_NAME avnd_test_multislider)

### Modifiers & file/dynamic ports (§1, §8) ###
avnd_make_all(TARGET TestAccurate      MAIN_FILE examples/Tests/TestAccurate.hpp      MAIN_CLASS examples::tests::TestAccurate      C_NAME avnd_test_accurate)
avnd_make_all(TARGET TestSmoothSlider  MAIN_FILE examples/Tests/TestSmoothSlider.hpp  MAIN_CLASS examples::tests::TestSmoothSlider  C_NAME avnd_test_smooth)
avnd_make_all(TARGET TestSoundfilePort MAIN_FILE examples/Tests/TestSoundfilePort.hpp MAIN_CLASS examples::tests::TestSoundfilePort C_NAME avnd_test_soundfile)
avnd_make_all(TARGET TestMidifilePort  MAIN_FILE examples/Tests/TestMidifilePort.hpp  MAIN_CLASS examples::tests::TestMidifilePort  C_NAME avnd_test_midifile)
avnd_make_all(TARGET TestDynamicPort   MAIN_FILE examples/Tests/TestDynamicPort.hpp   MAIN_CLASS examples::tests::TestDynamicPort   C_NAME avnd_test_dynamic_port)

### Threading & MIDI out (§5, §9) ###
avnd_make_all(TARGET TestWorker  MAIN_FILE examples/Tests/TestWorker.hpp  MAIN_CLASS examples::tests::TestWorker  C_NAME avnd_test_worker)
avnd_make_all(TARGET TestMidiOut MAIN_FILE examples/Tests/TestMidiOut.hpp MAIN_CLASS examples::tests::TestMidiOut C_NAME avnd_test_midi_out)
