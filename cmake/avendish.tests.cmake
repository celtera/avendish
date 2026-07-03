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
  endif()
endif()

