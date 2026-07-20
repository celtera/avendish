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
        GIT_TAG        v3.15.2
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

  # Custom-state feature (STATE_SUPPORT_DESIGN.md)
  avnd_add_static_test(test_state tests/test_state.cpp)

  if(AVND_ENABLE_CLAP AND CLAP_HEADER)
    # clap_plugin_params::flush event application.
    avnd_add_catch_test(test_params_flush tests/objects/params_flush.cpp)
    target_include_directories(test_params_flush PRIVATE "${CLAP_HEADER}")


    avnd_add_catch_test(test_state_clap tests/objects/state_clap.cpp)
    target_include_directories(test_state_clap PRIVATE "${CLAP_HEADER}")

    # Transport-to-tick conversion (uses the clap headers for the clap
    # converter; the vst3 converter is tested through a structural mirror).
    avnd_add_catch_test(test_transport tests/test_transport.cpp)
    target_include_directories(test_transport PRIVATE "${CLAP_HEADER}")

    # update() dispatch on host parameter changes.
    avnd_add_catch_test(test_update_controls tests/objects/update_controls.cpp)
    target_include_directories(test_update_controls PRIVATE "${CLAP_HEADER}")
  endif()
endif()

