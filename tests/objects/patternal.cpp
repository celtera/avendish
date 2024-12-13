#include <catch2/catch_all.hpp>

#include <examples/Advanced/Patternal/Patternal.hpp>

// Tests for the Patternal object
TEST_CASE("Test Patternal patterns", "[advanced][patternal]")
{
  SECTION("The input pattern is stored correctly") {
    // Instanciate the object:
    patternal::Processor patternalProcessor;

    // Create the input pattern:
    patternalProcessor.inputs.patterns.value = {
      (patternal::Pattern) {42, {127, 127, 127, 127}}, // hi-hat
      (patternal::Pattern) {38, {0, 127, 0, 127}}, // snare
      (patternal::Pattern) {35, {127, 0, 127, 0}}, // bass drum
    };

    // Check that the input pattern is stored correctly:
    REQUIRE(patternalProcessor.inputs.patterns.value[0].note == 42);
    REQUIRE(patternalProcessor.inputs.patterns.value[0].pattern[0] == 127);
    REQUIRE(patternalProcessor.inputs.patterns.value[1].note == 38);
    REQUIRE(patternalProcessor.inputs.patterns.value[1].pattern[0] == 0);
    REQUIRE(patternalProcessor.inputs.patterns.value[2].note == 35);
    REQUIRE(patternalProcessor.inputs.patterns.value[2].pattern[0] == 127);
  }
}
