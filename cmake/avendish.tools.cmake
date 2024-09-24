
# Demo: dump all the known metadata.
if(TARGET pantor::inja AND TARGET nlohmann_json::nlohmann_json)
  add_executable(json_to_maxref examples/Demos/JSONToMaxref.cpp)
  if(NOT MSVC)
    target_compile_options(json_to_maxref PRIVATE -std=c++20)
  endif()

  target_link_libraries(json_to_maxref PRIVATE nlohmann_json::nlohmann_json pantor::inja)
endif()

# Demo: generate matching pd help patches
add_executable(generate_pd_help examples/Demos/GeneratePdHelp.cpp)
target_link_libraries(generate_pd_help PRIVATE Avendish)
