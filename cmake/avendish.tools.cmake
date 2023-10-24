
# Demo: dump all the known metadata.
add_executable(json_to_maxref examples/Demos/JSONToMaxref.cpp)
target_link_libraries(json_to_maxref PRIVATE nlohmann_json::nlohmann_json pantor::inja)

# Demo: generate matching pd help patches
add_executable(generate_pd_help examples/Demos/GeneratePdHelp.cpp)
target_link_libraries(generate_pd_help PRIVATE Avendish)
