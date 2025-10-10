# TouchDesigner binding CMake configuration
# Following the pattern of avendish.max.cmake and avendish.pd.cmake

if(NOT TOUCHDESIGNER_CPP_SAMPLES_PATH)
  # Allow setting via environment variable or cache
  set(TOUCHDESIGNER_CPP_SAMPLES_PATH "" CACHE PATH "Path to TouchDesigner CustomOperatorSamples directory")
endif()

# Function to create a TouchDesigner CHOP from an Avendish processor
function(avnd_make_touchdesigner)
  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "LINK_LIBRARIES" ${ARGN})

  if(NOT TARGET "${AVND_TARGET}")
    message(STATUS "Will not build ${AVND_TARGET} (touchdesigner): target does not exist")
    return()
  endif()

  if(NOT TOUCHDESIGNER_CPP_SAMPLES_PATH)
    message(STATUS "Will not build ${AVND_TARGET} (touchdesigner): TOUCHDESIGNER_CPP_SAMPLES_PATH not set")
    return()
  endif()

  if(NOT IS_DIRECTORY "${TOUCHDESIGNER_CPP_SAMPLES_PATH}")
    message(STATUS "Will not build ${AVND_TARGET} (touchdesigner): TOUCHDESIGNER_CPP_SAMPLES_PATH does not exist")
    return()
  endif()

  # Find the CPlusPlus_Common.h header
  set(TD_COMMON_HEADER "${TOUCHDESIGNER_CPP_SAMPLES_PATH}/CHOP/BasicFilterCHOP/CPlusPlus_Common.h")
  if(NOT EXISTS "${TD_COMMON_HEADER}")
    message(STATUS "Will not build ${AVND_TARGET} (touchdesigner): CPlusPlus_Common.h not found")
    return()
  endif()

  # Determine plugin type based on processor capabilities
  # For now, we only support CHOP (audio processors)
  set(TD_TYPE "CHOP")
  set(TD_BASE_HEADER "${TOUCHDESIGNER_CPP_SAMPLES_PATH}/CHOP/BasicFilterCHOP/CHOP_CPlusPlusBase.h")

  if(NOT EXISTS "${TD_BASE_HEADER}")
    message(STATUS "Will not build ${AVND_TARGET} (touchdesigner): ${TD_TYPE}_CPlusPlusBase.h not found")
    return()
  endif()

  # Generate the binding cpp file from prototype template
  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_touchdesigner.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  # Create the plugin library
  set(AVND_FX_TARGET "avnd_touchdesigner_${AVND_C_NAME}")

  add_library(${AVND_FX_TARGET} MODULE)

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_touchdesigner.cpp"
  )

  # Set output name (TouchDesigner plugins are .dll on Windows, .dylib on macOS)
  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      OUTPUT_NAME "${AVND_C_NAME}"
      PREFIX ""  # No lib prefix
      CXX_STANDARD 23
      CXX_EXTENSIONS OFF
  )

  # Add TouchDesigner headers
  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      "${TOUCHDESIGNER_CPP_SAMPLES_PATH}/CHOP/BasicFilterCHOP"
  )

  # Link to Avendish and dependencies
  target_link_libraries(
    ${AVND_FX_TARGET}
    PUBLIC
      Avendish::Avendish
      Boost::boost
  )

  if(AVND_LINK_LIBRARIES)
    target_link_libraries(${AVND_FX_TARGET} PRIVATE ${AVND_LINK_LIBRARIES})
  endif()

  # Platform-specific settings
  if(APPLE)
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        SUFFIX ".dylib"
        BUNDLE FALSE
        MACOSX_BUNDLE FALSE
    )
  elseif(WIN32)
    set_target_properties(
      ${AVND_FX_TARGET}
      PROPERTIES
        SUFFIX ".dll"
    )

    # Windows-specific: ensure DLLEXPORT is defined
    target_compile_definitions(${AVND_FX_TARGET} PRIVATE DLLEXPORT=__declspec(dllexport))
  endif()

  # Add Avendish headers
  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_SOURCE_DIR}/include"
  )

  # Compiler flags
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${AVND_FX_TARGET} PRIVATE -fvisibility=hidden)
  endif()

  # Installation (optional)
  # TouchDesigner plugins typically go in <Project>/Plugins/
  # install(TARGETS ${AVND_FX_TARGET} LIBRARY DESTINATION "Plugins")

  message(STATUS "Configured TouchDesigner CHOP: ${AVND_FX_TARGET}")
endfunction()
