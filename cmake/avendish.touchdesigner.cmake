option(TOUCHDESIGNER_SDK_PATH "Path to TouchDesigner CustomOperatorSamples directory" "")
if(NOT TOUCHDESIGNER_SDK_PATH)
  function(avnd_make_touchdesigner)
  endfunction()
  return()
endif()

# Function to create a TouchDesigner Custom operator OP from an Avendish processor
function(avnd_make_touchdesigner)
  cmake_parse_arguments(AVND "" "OPTYPE;TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "LINK_LIBRARIES" ${ARGN})

  if(MINGW OR CYGWIN OR MSYS OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(FATAL_ERROR "Will not build ${AVND_TARGET} (touchdesigner): MinGW is not supported, use a compiler compatible with the MSVC ABI.")
    return()
  endif()

  if(NOT IS_DIRECTORY "${TOUCHDESIGNER_SDK_PATH}")
    message(FATAL_ERROR "Will not build ${AVND_TARGET} (touchdesigner): TOUCHDESIGNER_SDK_PATH does not exist")
    return()
  endif()

  # Find the CPlusPlus_Common.h header
  set(TD_COMMON_HEADER "${TOUCHDESIGNER_SDK_PATH}/include/CPlusPlus_Common.h")
  if(NOT EXISTS "${TD_COMMON_HEADER}")
    message(FATAL_ERROR "Will not build ${AVND_TARGET} (touchdesigner): CPlusPlus_Common.h not found")
    return()
  endif()

  # Generate the binding cpp file from prototype template
  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/${AVND_OPTYPE}.prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_touchdesigner.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  # Create the plugin library
  set(AVND_FX_TARGET "${AVND_TARGET}_${AVND_OPTYPE}_td")

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
      LIBRARY_OUTPUT_DIRECTORY td
      RUNTIME_OUTPUT_DIRECTORY td
      PREFIX ""  # No lib prefix
      CXX_STANDARD 23
      CXX_EXTENSIONS OFF
  )

  # Add TouchDesigner headers
  target_include_directories(
    ${AVND_FX_TARGET}
    SYSTEM
    PRIVATE
      "${TOUCHDESIGNER_SDK_PATH}/include"
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
  endif()

  # Add Avendish headers
  target_include_directories(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_SOURCE_DIR}/include"
  )

  # Compiler flags
  if(MSVC)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${AVND_FX_TARGET} PRIVATE -fvisibility=hidden)
  endif()

  # Installation (optional)
  # TouchDesigner plugins typically go in <Project>/Plugins/
  # install(TARGETS ${AVND_FX_TARGET} LIBRARY DESTINATION "Plugins")

  message(STATUS "Configured TouchDesigner: ${AVND_FX_TARGET}")
endfunction()

target_sources(Avendish PRIVATE
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/chop/audio_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/chop/message_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/CHOP_AUDIO.prototype.cpp.in"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/CHOP_MESSAGE.prototype.cpp.in"

  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/top/texture_processor.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/TOP.prototype.cpp.in"

  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/all.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/configure.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/helpers.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/parameter_setup.hpp"
  "${AVND_SOURCE_DIR}/include/avnd/binding/touchdesigner/parameter_update.hpp"
)
