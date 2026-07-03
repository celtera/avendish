include(CTest)

function(avnd_add_static_test theTarget theFile)
  add_library("${theTarget}" STATIC "${theFile}")
  avnd_common_setup("" "${theTarget}")
endfunction()

function(avnd_add_executable_test theTarget theFile)
  add_executable("${theTarget}" "${theFile}")
  add_test(NAME "${theTarget}" COMMAND "${theTarget}")
  avnd_common_setup("" "${theTarget}")
endfunction()

function(avnd_add_catch_test theTarget theFile)
  add_executable("${theTarget}" "${theFile}")
  add_test(NAME "${theTarget}" COMMAND "${theTarget}")
  avnd_common_setup("" "${theTarget}")
  target_link_libraries("${theTarget}" PRIVATE Catch2::Catch2WithMain)
endfunction()

if(BUILD_TESTING)
  if(NOT TARGET Catch2::Catch2WithMain)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.15.1
    )
    
    FetchContent_MakeAvailable(Catch2)
    if(NOT TARGET Catch2::Catch2WithMain)
        message(WARNING "libremidi: Catch2::Catch2WithMain target not found")
        return()
    endif()
  endif()


  avnd_add_static_test(test_introspection tests/test_introspection.cpp)
  avnd_add_static_test(test_introspection_rec tests/test_introspection_rec.cpp)
  avnd_add_static_test(test_predicate tests/test_predicate.cpp)
  avnd_add_static_test(test_vintage tests/tests_vintage.cpp)
  if(TARGET concurrentqueue) # vintage gui glue's bus transport
    target_link_libraries(test_vintage PRIVATE concurrentqueue)
  endif()
  avnd_add_static_test(test_channels tests/tests_channels.cpp)
  avnd_add_static_test(test_function_reflection tests/tests_function_reflection.cpp)
  avnd_add_static_test(test_audioprocessor tests/test_audioprocessor.cpp)
  avnd_add_static_test(test_index_in_struct tests/test_index_in_struct.cpp)

  avnd_add_executable_test(test_introspection_many tests/test_introspection_many.cpp)
  avnd_add_executable_test(test_reflection tests/test_reflection.cpp)

  avnd_add_catch_test(test_gain tests/objects/gain.cpp)
  avnd_add_catch_test(test_patternal tests/objects/patternal.cpp)

  if(TARGET Avendish::soft_ui)
    avnd_add_catch_test(test_soft_ui tests/ui/test_soft_ui.cpp)
    avnd_target_soft_ui(test_soft_ui)

    # End-to-end clap.gui test: build a .clap with an editor, then a host
    # that loads and drives it.
    if(WIN32 AND COMMAND avnd_make_clap AND TARGET avnd_pugl)
      avnd_make_clap(
        TARGET ClapUiPlugTest
        MAIN_FILE tests/ui/ClapUiPlug.hpp
        MAIN_CLASS avnd_test::ClapUiPlug
        C_NAME avnd_clap_ui_test
      )
      if(TARGET ClapUiPlugTest_clap)
        avnd_add_catch_test(test_clap_gui tests/ui/test_clap_gui.cpp)
        add_dependencies(test_clap_gui ClapUiPlugTest_clap)
        target_include_directories(test_clap_gui PRIVATE ${CLAP_HEADER})
        target_compile_definitions(test_clap_gui PRIVATE
          "AVND_TEST_CLAP_PATH=\"$<TARGET_FILE:ClapUiPlugTest_clap>\"")
      endif()
    endif()

    # Standalone preview smoke test: open the editor in a real top-level
    # window for a few frames and exit cleanly.
    if(WIN32 AND TARGET HelpersUiPreview_ui_preview)
      add_test(NAME test_ui_preview
               COMMAND HelpersUiPreview_ui_preview --frames 30)
    endif()

    # Tier C: the CustomUiWindow example's author-provided editor.
    if(WIN32 AND TARGET CustomUiWindow_clap)
      avnd_add_catch_test(test_custom_ui_window tests/ui/test_custom_ui_window.cpp)
      add_dependencies(test_custom_ui_window CustomUiWindow_clap)
      target_include_directories(test_custom_ui_window PRIVATE ${CLAP_HEADER})
      target_compile_definitions(test_custom_ui_window PRIVATE
        "AVND_TEST_CUSTOM_UI_CLAP_PATH=\"$<TARGET_FILE:CustomUiWindow_clap>\"")
    endif()

    # Same plug-in through the VST3 editor glue.
    if(WIN32 AND COMMAND avnd_make_vst3 AND TARGET avnd_pugl AND VST3_SDK_ROOT)
      avnd_make_vst3(
        TARGET Vst3UiPlugTest
        MAIN_FILE tests/ui/ClapUiPlug.hpp
        MAIN_CLASS avnd_test::ClapUiPlug
        C_NAME avnd_vst3_ui_test
      )
      if(TARGET Vst3UiPlugTest_vst3)
        avnd_add_catch_test(test_vst3_gui tests/ui/test_vst3_gui.cpp)
        add_dependencies(test_vst3_gui Vst3UiPlugTest_vst3)
        target_include_directories(test_vst3_gui PRIVATE ${VST3_SDK_ROOT})
        target_link_libraries(test_vst3_gui PRIVATE pluginterfaces)
        target_compile_definitions(test_vst3_gui PRIVATE
          "AVND_TEST_VST3_PATH=\"$<TARGET_FILE:Vst3UiPlugTest_vst3>\"")
      endif()
    endif()

    # Same plug-in through the VST2 (vintage) editor glue.
    if(WIN32 AND COMMAND avnd_make_vintage AND TARGET avnd_pugl)
      avnd_make_vintage(
        TARGET VintageUiPlugTest
        MAIN_FILE tests/ui/ClapUiPlug.hpp
        MAIN_CLASS avnd_test::ClapUiPlug
        C_NAME avnd_vintage_ui_test
      )
      if(TARGET VintageUiPlugTest_vintage)
        avnd_add_catch_test(test_vintage_gui tests/ui/test_vintage_gui.cpp)
        add_dependencies(test_vintage_gui VintageUiPlugTest_vintage)
        target_compile_definitions(test_vintage_gui PRIVATE
          "AVND_TEST_VST2_PATH=\"$<TARGET_FILE:VintageUiPlugTest_vintage>\"")
      endif()
    endif()
  endif()
endif()

