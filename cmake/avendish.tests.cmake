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
  endif()

  # The end-to-end editor tests need a real windowing system: Win32, or
  # X11 (run under Xvfb in headless CI).
  if(WIN32)
    set(AVND_GUI_TESTS 1)
  elseif(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
    if(X11_FOUND)
      set(AVND_GUI_TESTS 1)
    else()
      set(AVND_GUI_TESTS 0)
    endif()
  else()
    set(AVND_GUI_TESTS 0)
  endif()

  # Editor test hosts drive the plug-in's window from outside on X11.
  function(avnd_gui_test_platform_libs theTarget)
    if(UNIX AND NOT APPLE)
      target_link_libraries("${theTarget}" PRIVATE X11::X11)
    endif()
  endfunction()

  # Tier C: the CustomUiWindow example's author-provided editor.
  if(AVND_GUI_TESTS AND TARGET CustomUiWindow_clap)
    avnd_add_catch_test(test_custom_ui_window tests/ui/test_custom_ui_window.cpp)
    add_dependencies(test_custom_ui_window CustomUiWindow_clap)
    target_include_directories(test_custom_ui_window PRIVATE ${CLAP_HEADER})
    avnd_gui_test_platform_libs(test_custom_ui_window)
    target_compile_definitions(test_custom_ui_window PRIVATE
      "AVND_TEST_CUSTOM_UI_CLAP_PATH=\"$<TARGET_FILE:CustomUiWindow_clap>\"")
  endif()
endif()

