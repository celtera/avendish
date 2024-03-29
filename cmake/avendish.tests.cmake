include(CTest)

function(avnd_add_static_test theTarget theFile)
  add_library("${theTarget}" STATIC "${theFile}")
  avnd_common_setup("" "${theTarget}")
endfunction()

function(avnd_add_executable_test theTarget theFile)
  add_executable("${theTarget}" "${theFile}")
  avnd_common_setup("" "${theTarget}")
endfunction()

if(BUILD_TESTING)
  avnd_add_static_test(test_introspection tests/test_introspection.cpp)
  avnd_add_static_test(test_introspection_rec tests/test_introspection_rec.cpp)
  avnd_add_static_test(test_predicate tests/test_predicate.cpp)
  avnd_add_static_test(test_vintage tests/tests_vintage.cpp)
  avnd_add_static_test(test_channels tests/tests_channels.cpp)
  avnd_add_static_test(test_function_reflection tests/tests_function_reflection.cpp)
  avnd_add_static_test(test_audioprocessor tests/test_audioprocessor.cpp)
  avnd_add_static_test(test_index_in_struct tests/test_index_in_struct.cpp)

  avnd_add_executable_test(test_introspection_many tests/test_introspection_many.cpp)
endif()
