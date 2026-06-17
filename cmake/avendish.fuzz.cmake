# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fuzz-testing harness: builds one libFuzzer executable per processor, the
# counterpart of the `dump` harness. Each target feeds a coverage-guided byte
# stream into every input port (parameters, MIDI, messages) and runs the node
# under the requested sanitizers.
#
# libFuzzer and <fuzzer/FuzzedDataProvider.h> are Clang features, so the
# backend self-skips on every other compiler.

set(AVND_FUZZ_SANITIZERS "address,undefined" CACHE STRING
    "Sanitizers combined with the libFuzzer engine in the fuzz harness")

# Per-object smoke-fuzz budget used when the targets are registered as CTest
# tests (so `ctest` fuzzes every object). CI can raise these for deep runs.
set(AVND_FUZZ_TEST_RUNS     "500000" CACHE STRING "libFuzzer -runs for ctest fuzz tests")
set(AVND_FUZZ_TEST_MAX_TIME "10"     CACHE STRING "libFuzzer -max_total_time (s) for ctest fuzz tests")
set(AVND_FUZZ_TEST_TIMEOUT  "20"     CACHE STRING "libFuzzer per-input -timeout (s); catches DoS/hang inputs")

function(avnd_make_fuzz)
  if(NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
    return()
  endif()

  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;C_NAME" "" ${ARGN})

  set(AVND_FX_TARGET "${AVND_TARGET}_fuzz")
  if(TARGET "${AVND_FX_TARGET}")
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${AVND_MAIN_CLASS}" MAIN_OUT_FILE)

  configure_file(
    "${AVND_SOURCE_DIR}/include/avnd/binding/fuzz/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_fuzz.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  add_executable(${AVND_FX_TARGET})

  set_target_properties(
    ${AVND_FX_TARGET}
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY fuzz
      RUNTIME_OUTPUT_DIRECTORY fuzz
  )

  target_sources(
    ${AVND_FX_TARGET}
    PRIVATE
      "${AVND_MAIN_FILE}"
      "${CMAKE_BINARY_DIR}/${MAIN_OUT_FILE}_fuzz.cpp"
  )

  target_link_libraries(
    ${AVND_FX_TARGET}
    PRIVATE
      Avendish::Avendish_fuzz
  )

  # libFuzzer engine + sanitizers, applied to both compile and link of this
  # target so the processor source itself is instrumented.
  set(_avnd_fuzz_flags
      -g -O1 -fno-omit-frame-pointer
      "-fsanitize=fuzzer,${AVND_FUZZ_SANITIZERS}")
  target_compile_options(${AVND_FX_TARGET} PRIVATE ${_avnd_fuzz_flags})
  target_link_options(${AVND_FX_TARGET} PRIVATE ${_avnd_fuzz_flags})

  avnd_common_setup("${AVND_TARGET}" "${AVND_FX_TARGET}")

  # Automatic fuzzing: every registered object becomes a `ctest` fuzz test, so a
  # newly added example is fuzzed with no extra work. `-error_exitcode=1` turns
  # any sanitizer finding / crash into a test failure; output is silenced.
  if(BUILD_TESTING)
    add_test(
      NAME "fuzz_${AVND_TARGET}"
      COMMAND ${AVND_FX_TARGET}
        -runs=${AVND_FUZZ_TEST_RUNS}
        -max_total_time=${AVND_FUZZ_TEST_MAX_TIME}
        -timeout=${AVND_FUZZ_TEST_TIMEOUT}
        -rss_limit_mb=4096
        -close_fd_mask=3
        -error_exitcode=1)
    # Wall-clock guard well above -max_total_time so a pathological single input
    # (e.g. an unbounded loop) fails the test instead of hanging CI forever.
    math(EXPR _avnd_fuzz_timeout "${AVND_FUZZ_TEST_MAX_TIME} * 4 + 30")
    set_tests_properties("fuzz_${AVND_TARGET}" PROPERTIES
      LABELS "fuzz"
      TIMEOUT "${_avnd_fuzz_timeout}")
  endif()
endfunction()

add_library(Avendish_fuzz INTERFACE)
target_link_libraries(Avendish_fuzz INTERFACE Avendish)
add_library(Avendish::Avendish_fuzz ALIAS Avendish_fuzz)
